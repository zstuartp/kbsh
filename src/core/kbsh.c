/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(HAVE_POSIX_SPAWN) && HAVE_POSIX_SPAWN
#include <spawn.h>
extern char **environ;
#endif

#include "localize.h"

#include "builtin/builtin.h"
#include "core/arena.h"
#include "core/buffer.h"
#include "core/env.h"
#include "core/pipeline.h"
#include "core/input.h"
#include "core/kbsh.h"
#include "core/parse.h"
#include "core/prompt.h"
#include "core/sig.h"

char *program_name = PACKAGE;
void (*kbsh_clean)(void);
char **kbsh_positional_params = NULL;
int kbsh_positional_param_count = 0;

enum kbsh_state_id {
	KBSH_STATE_INIT,
	KBSH_STATE_READ,
	KBSH_STATE_READ_MORE,
	KBSH_STATE_PARSE,
	KBSH_STATE_EXEC,
	KBSH_STATE_CLEANUP,
	KBSH_STATE_EXIT,
	KBSH_STATE_COUNT
};

enum kbsh_event_id {
	KBSH_EVENT_OKAY,
	KBSH_EVENT_SKIP,
	KBSH_EVENT_NEED_MORE,
	KBSH_EVENT_END_OF_FILE,
	KBSH_EVENT_PARSE_ERROR,
	KBSH_EVENT_EXEC_ERROR,
	KBSH_EVENT_FATAL,
	KBSH_EVENT_COUNT
};

struct kbsh_state {
	enum kbsh_state_id    state_id;
	enum kbsh_event_id    event_id;
	enum kbsh_run_mode_id run_mode_id;
	int                   last_command_status;
	size_t                arena_mark;
	struct Buffer         staging;
	struct kbsh_pipeline  pipeline;
};

static enum kbsh_event_id get_input(struct kbsh_state *,
				    FILE *,
				    FILE *,
				    struct kbsh_arena *);
static enum kbsh_event_id parse_input(struct kbsh_state *, struct kbsh_arena *);
static enum kbsh_event_id exec_cmd(struct kbsh_state *, struct kbsh_arena *);
static enum kbsh_event_id do_cleanup(struct kbsh_state *, struct kbsh_arena *);
static enum kbsh_state_id kbsh_transition(const struct kbsh_state *);

static int          kbsh_exec_argv(char **argv);
static void         kbsh_fork(struct kbsh_cmd *cmd);
static int          kbsh_apply_redirs(const struct kbsh_cmd *cmd);
static void         kbsh_exec_single_with_redirs(struct kbsh_cmd *cmd,
						 struct kbsh_arena *arena);
static void         kbsh_exec_pipeline(struct kbsh_pipeline *pl,
					struct kbsh_arena *arena);
static char        *kbsh_run_read_line(FILE *fp);
static int          kbsh_wait_status_code(int wait_status);

void kbsh_init(void)
{
	kbsh_sig_init();
	kbsh_env_init();
}

void kbsh_exit(int exit_status)
{
	if (kbsh_clean)
		kbsh_clean();
	kbsh_env_exit();
	exit(exit_status);
}

int kbsh_run(enum kbsh_run_mode_id mode, FILE *in, FILE *out)
{
	static unsigned char pool[KBSH_POOL_SIZE];
	struct kbsh_arena arena;
	struct kbsh_state state;

	memset(&state, 0, sizeof(state));
	state.state_id = KBSH_STATE_READ;
	state.run_mode_id = mode;

	if (kbsh_arena_init(&arena, pool, sizeof(pool)) != KBSH_ARENA_SUCCESS) {
		kbsh_exit(1);
	}

	if (mode == KBSH_RUN_MODE_INTERACTIVE)
		kbsh_env_update();

	while (state.state_id != KBSH_STATE_EXIT) {
		switch (state.state_id) {
		case KBSH_STATE_READ:
			state.arena_mark = kbsh_arena_mark(&arena);
			state.event_id = get_input(&state, in, out, &arena);
			break;
		case KBSH_STATE_READ_MORE:
			state.event_id = get_input(&state, in, out, &arena);
			break;
		case KBSH_STATE_PARSE:
			state.event_id = parse_input(&state, &arena);
			break;
		case KBSH_STATE_EXEC:
			state.event_id = exec_cmd(&state, &arena);
			break;
		case KBSH_STATE_CLEANUP:
			state.event_id = do_cleanup(&state, &arena);
			break;
		default:
			return 1;
		}
		state.state_id = kbsh_transition(&state);
	}

	return state.last_command_status;
}

static enum kbsh_event_id get_input(struct kbsh_state *state,
				    FILE *in,
				    FILE *out,
				    struct kbsh_arena *arena)
{
	char *line = NULL;
	unsigned char *arena_buf = NULL;
	size_t len;
	size_t old_len;
	const char *rl_prompt;

	(void)out;

	if (state->run_mode_id == KBSH_RUN_MODE_INTERACTIVE) {
		rl_prompt = (state->state_id == KBSH_STATE_READ_MORE)
				? prompt.scnd_ch
				: prompt.crnt_ch;
		line = kbsh_input_readline(rl_prompt);
		if (!line)
			printf("exit\n");
	} else {
		line = kbsh_run_read_line(in);
	}

	if (state->state_id == KBSH_STATE_READ_MORE) {
		if (!line) {
			/* EOF while expecting continuation */
			fprintf(stderr, "%s: ", program_name);
			fprintf(stderr, _("syntax error: "));
			fprintf(stderr, "%s\n", _("unexpected EOF"));
			state->last_command_status =
			    (int)KBSH_PARSE_ERROR_UNEXPECTED_EOF;
			return KBSH_EVENT_PARSE_ERROR;
		}
		old_len = strlen(state->staging.full);
		len = strlen(line);
		if (kbsh_arena_alloc(arena, old_len + len + 1, 1, &arena_buf) !=
		    KBSH_ARENA_SUCCESS)
			kbsh_exit(1);
		memcpy(arena_buf, state->staging.full, old_len);
		memcpy(arena_buf + old_len, line, len + 1);
		state->staging.full = (char *)arena_buf;
		state->staging.full_size = old_len + len + 1;
		return KBSH_EVENT_OKAY;
	}

	if (!line)
		return KBSH_EVENT_END_OF_FILE;

	if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
		return KBSH_EVENT_SKIP;

	len = strlen(line);
	if (kbsh_arena_alloc(arena, len + 1, 1, &arena_buf) !=
	    KBSH_ARENA_SUCCESS)
		kbsh_exit(1);
	memcpy(arena_buf, line, len + 1);
	state->staging.full = (char *)arena_buf;
	state->staging.full_size = len + 1;
	return KBSH_EVENT_OKAY;
}

static enum kbsh_event_id parse_input(struct kbsh_state *state,
				      struct kbsh_arena *arena)
{
	enum kbsh_parse_result result;

	result = kbsh_parse(&state->pipeline,
			    &state->staging,
			    arena,
			    state->last_command_status);
	switch (result) {
	case KBSH_PARSE_OK:
		return KBSH_EVENT_OKAY;
	case KBSH_PARSE_NEED_MORE:
		return KBSH_EVENT_NEED_MORE;
	default:
		state->last_command_status = (int)result;
		return KBSH_EVENT_PARSE_ERROR;
	}
}

static enum kbsh_event_id exec_cmd(struct kbsh_state *state,
				   struct kbsh_arena *arena)
{
	kbsh_main(&state->pipeline, arena);
	state->last_command_status = 0;
	if (state->run_mode_id == KBSH_RUN_MODE_INTERACTIVE)
		kbsh_input_save_history();
	return KBSH_EVENT_OKAY;
}

static enum kbsh_event_id do_cleanup(struct kbsh_state *state,
				     struct kbsh_arena *arena)
{
	(void)kbsh_arena_rewind(arena, state->arena_mark);
	memset(&state->staging, 0, sizeof(state->staging));
	memset(&state->pipeline, 0, sizeof(state->pipeline));
	return KBSH_EVENT_OKAY;
}

/*
 * Transition table: [run_mode][current_state][event] -> next_state
 */
/* clang-format off */
static const enum kbsh_state_id
kbsh_transitions[2][KBSH_STATE_COUNT][KBSH_EVENT_COUNT] = {
	[KBSH_RUN_MODE_NONINTERACTIVE] = {
		[KBSH_STATE_READ] = {
			KBSH_STATE_PARSE,     /* OKAY      */
			KBSH_STATE_READ,      /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_READ_MORE] = {
			KBSH_STATE_PARSE,     /* OKAY      */
			KBSH_STATE_READ_MORE, /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_PARSE] = {
			KBSH_STATE_EXEC,      /* OKAY      */
			KBSH_STATE_CLEANUP,   /* SKIP      */
			KBSH_STATE_READ_MORE, /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_EXEC] = {
			KBSH_STATE_CLEANUP,   /* OKAY      */
			KBSH_STATE_EXIT,      /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_CLEANUP,   /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_CLEANUP] = {
			KBSH_STATE_READ,      /* OKAY      */
			KBSH_STATE_EXIT,      /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
	},
	[KBSH_RUN_MODE_INTERACTIVE] = {
		[KBSH_STATE_READ] = {
			KBSH_STATE_PARSE,     /* OKAY      */
			KBSH_STATE_READ,      /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_READ_MORE] = {
			KBSH_STATE_PARSE,     /* OKAY      */
			KBSH_STATE_READ_MORE, /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_PARSE] = {
			KBSH_STATE_EXEC,      /* OKAY      */
			KBSH_STATE_CLEANUP,   /* SKIP      */
			KBSH_STATE_READ_MORE, /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_CLEANUP,   /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_EXEC] = {
			KBSH_STATE_CLEANUP,   /* OKAY      */
			KBSH_STATE_EXIT,      /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_CLEANUP,   /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
		[KBSH_STATE_CLEANUP] = {
			KBSH_STATE_READ,      /* OKAY      */
			KBSH_STATE_EXIT,      /* SKIP      */
			KBSH_STATE_EXIT,      /* NEED_MORE */
			KBSH_STATE_EXIT,      /* EOF       */
			KBSH_STATE_EXIT,      /* PARSE_ERR */
			KBSH_STATE_EXIT,      /* EXEC_ERR  */
			KBSH_STATE_EXIT,      /* FATAL     */
		},
	},
};
/* clang-format on */

static enum kbsh_state_id kbsh_transition(const struct kbsh_state *state)
{
	if (state->state_id >= KBSH_STATE_EXIT) {
		return KBSH_STATE_EXIT;
	}
	return kbsh_transitions[state->run_mode_id][state->state_id]
			       [state->event_id];
}

static int is_assignment(const char *word)
{
	const char *p;
	if (!word || !*word)
		return 0;
	if (!isalpha((unsigned char)*word) && *word != '_')
		return 0;
	for (p = word + 1; *p && *p != '='; p++) {
		if (!isalnum((unsigned char)*p) && *p != '_')
			return 0;
	}
	return *p == '=';
}

static void do_assignment(const char *word)
{
	char name[256];
	const char *eq;
	size_t name_len;

	eq = strchr(word, '=');
	if (!eq)
		return;
	name_len = (size_t)(eq - word);
	if (name_len >= sizeof(name))
		return;
	memcpy(name, word, name_len);
	name[name_len] = '\0';
	setenv(name, eq + 1, 1);
}

void kbsh_main(struct kbsh_pipeline *pl, struct kbsh_arena *arena)
{
	int i;
	struct kbsh_cmd *cmd;

	if (!pl || pl->ncmds == 0)
		return;

	cmd = &pl->cmds[0];

	/* Pure assignment: single command, no redirects, all words are VAR=val */
	if (pl->ncmds == 1 && cmd->nredirs == 0 && cmd->argc > 0) {
		for (i = 0; i < cmd->argc; i++) {
			if (!is_assignment(cmd->argv[i]))
				break;
		}
		if (i == cmd->argc) {
			for (i = 0; i < cmd->argc; i++)
				do_assignment(cmd->argv[i]);
			return;
		}
	}

	if (pl->ncmds == 1 && cmd->nredirs == 0) {
		/* Simple command: builtin inline or posix_spawnp */
		if (!kbsh_find_builtin(cmd, arena))
			kbsh_fork(cmd);
	} else if (pl->ncmds == 1) {
		/* Single command with redirects */
		kbsh_exec_single_with_redirs(cmd, arena);
	} else {
		/* Pipeline */
		kbsh_exec_pipeline(pl, arena);
	}
}

/* kbsh_exec_argv: exec helper used inside fork children. */
static int kbsh_exec_argv(char **argv)
{
	int err = execvp(argv[0], argv);

	if (err) {
		fprintf(stderr, "%s: ", program_name);
		perror(argv[0]);
	}
	return err;
}

/* kbsh_fork: execute a simple (no pipe, no redir) external command. */
static void kbsh_fork(struct kbsh_cmd *cmd)
{
#if defined(HAVE_POSIX_SPAWN) && HAVE_POSIX_SPAWN
	pid_t pid;
	int err =
	    posix_spawnp(&pid, cmd->argv[0], NULL, NULL, cmd->argv, environ);

	if (err != 0) {
		fprintf(stderr,
			"%s: %s: %s\n",
			program_name,
			cmd->argv[0],
			strerror(err));
		return;
	}
	waitpid(pid, NULL, 0);
#else
	pid_t pid = fork();

	if (!pid)
		_exit(kbsh_exec_argv(cmd->argv));
	else if (pid > 0)
		wait(NULL);
	if (pid < 0)
		kbsh_exit(errno);
#endif
}

/* kbsh_apply_redirs: dup2 file descriptors per the redirect list.
 * Returns 0 on success, -1 if any open() fails. */
static int kbsh_apply_redirs(const struct kbsh_cmd *cmd)
{
	int i, fd;

	for (i = 0; i < cmd->nredirs; i++) {
		switch (cmd->redirs[i].type) {
		case KBSH_REDIR_OUT:
			fd = open(cmd->redirs[i].target,
				  O_WRONLY | O_CREAT | O_TRUNC,
				  0644);
			if (fd < 0) {
				perror(cmd->redirs[i].target);
				return -1;
			}
			dup2(fd, STDOUT_FILENO);
			close(fd);
			break;
		case KBSH_REDIR_APPEND:
			fd = open(cmd->redirs[i].target,
				  O_WRONLY | O_CREAT | O_APPEND,
				  0644);
			if (fd < 0) {
				perror(cmd->redirs[i].target);
				return -1;
			}
			dup2(fd, STDOUT_FILENO);
			close(fd);
			break;
		case KBSH_REDIR_IN:
			fd = open(cmd->redirs[i].target, O_RDONLY);
			if (fd < 0) {
				perror(cmd->redirs[i].target);
				return -1;
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
			break;
		case KBSH_REDIR_ERR:
			fd = open(cmd->redirs[i].target,
				  O_WRONLY | O_CREAT | O_TRUNC,
				  0644);
			if (fd < 0) {
				perror(cmd->redirs[i].target);
				return -1;
			}
			dup2(fd, STDERR_FILENO);
			close(fd);
			break;
		case KBSH_REDIR_ERR_OUT:
			dup2(STDOUT_FILENO, STDERR_FILENO);
			break;
		}
	}
	return 0;
}

/* kbsh_exec_single_with_redirs: run one command with redirects.
 * For builtins: save/restore the affected fds around the call.
 * For externals: fork, apply redirects in the child, exec. */
static void kbsh_exec_single_with_redirs(struct kbsh_cmd *cmd,
					 struct kbsh_arena *arena)
{
	int saved[3] = {-1, -1, -1};
	int i, target_fd;
	pid_t pid;

	/* Save fds that will be redirected */
	for (i = 0; i < cmd->nredirs; i++) {
		switch (cmd->redirs[i].type) {
		case KBSH_REDIR_IN:
			target_fd = STDIN_FILENO;
			break;
		case KBSH_REDIR_OUT:
		case KBSH_REDIR_APPEND:
			target_fd = STDOUT_FILENO;
			break;
		case KBSH_REDIR_ERR:
		case KBSH_REDIR_ERR_OUT:
			target_fd = STDERR_FILENO;
			break;
		default:
			continue;
		}
		if (saved[target_fd] == -1)
			saved[target_fd] = dup(target_fd);
	}

	if (kbsh_apply_redirs(cmd) < 0)
		goto restore;

	if (!kbsh_find_builtin(cmd, arena)) {
		/* External: fork, child inherits the redirected fds */
		pid = fork();
		if (pid == 0) {
			for (i = 0; i < 3; i++)
				if (saved[i] != -1)
					close(saved[i]);
			_exit(kbsh_exec_argv(cmd->argv));
		} else if (pid > 0) {
			waitpid(pid, NULL, 0);
		} else {
			kbsh_exit(errno);
		}
	}

restore:
	for (i = 0; i < 3; i++) {
		if (saved[i] != -1) {
			dup2(saved[i], i);
			close(saved[i]);
		}
	}
}

/* kbsh_exec_pipeline: execute a multi-stage pipeline.
 * All stages are forked; builtins run in child processes (POSIX-correct).
 * Pipe fds are wired with dup2; per-stage redirects are applied in children. */
static void kbsh_exec_pipeline(struct kbsh_pipeline *pl,
				struct kbsh_arena *arena)
{
	int pipes[KBSH_PIPELINE_MAX - 1][2];
	pid_t pids[KBSH_PIPELINE_MAX];
	int i, j;

	for (i = 0; i < pl->ncmds - 1; i++) {
		if (pipe(pipes[i]) < 0)
			kbsh_exit(errno);
	}

	for (i = 0; i < pl->ncmds; i++) {
		pids[i] = fork();
		if (pids[i] < 0)
			kbsh_exit(errno);
		if (pids[i] == 0) {
			/* Wire pipe ends */
			if (i > 0)
				dup2(pipes[i - 1][0], STDIN_FILENO);
			if (i < pl->ncmds - 1)
				dup2(pipes[i][1], STDOUT_FILENO);
			/* Close all pipe fds in child */
			for (j = 0; j < pl->ncmds - 1; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}
			/* Per-stage redirects override pipe wiring */
			kbsh_apply_redirs(&pl->cmds[i]);
			/* Run builtin or exec external */
			if (kbsh_find_builtin(&pl->cmds[i], arena)) {
				fflush(NULL);
				_exit(0);
			}
			_exit(kbsh_exec_argv(pl->cmds[i].argv));
		}
	}

	/* Parent: close all pipe ends */
	for (i = 0; i < pl->ncmds - 1; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	/* Wait for all children */
	for (i = 0; i < pl->ncmds; i++)
		waitpid(pids[i], NULL, 0);
}

static char *kbsh_run_read_line(FILE *fp)
{
	static char s_line_buf[4096];

	if (!fgets(s_line_buf, sizeof(s_line_buf), fp))
		return NULL;
	return s_line_buf;
}

int kbsh_capture_command_output(const char *command,
				struct kbsh_arena *arena,
				char **output)
{
	unsigned char *arena_buf;
	char io_buf[4096];
	FILE *tmp_in;
	FILE *tmp_out;
	int pipe_fds[2];
	pid_t pid;
	int wait_status;
	size_t trimmed_size;
	size_t total_read;
	ssize_t nread;

	if (!command || !arena || !output)
		kbsh_exit(EINVAL);

	*output = NULL;
	tmp_in = tmpfile();
	tmp_out = tmpfile();
	if (!tmp_in || !tmp_out)
		kbsh_exit(errno ? errno : 1);

	if (fputs(command, tmp_in) == EOF || fputc('\n', tmp_in) == EOF ||
	    fflush(tmp_in) == EOF)
		kbsh_exit(errno ? errno : 1);
	if (fseek(tmp_in, 0, SEEK_SET) != 0)
		kbsh_exit(errno ? errno : 1);

	if (pipe(pipe_fds) < 0)
		kbsh_exit(errno);

	fflush(NULL);
	pid = fork();
	if (pid < 0)
		kbsh_exit(errno);

	if (pid == 0) {
		close(pipe_fds[0]);
		if (dup2(pipe_fds[1], STDOUT_FILENO) < 0)
			_exit(1);
		if (pipe_fds[1] != STDOUT_FILENO)
			close(pipe_fds[1]);
		wait_status = kbsh_run(KBSH_RUN_MODE_NONINTERACTIVE, tmp_in, stdout);
		fflush(NULL);
		_exit(wait_status);
	}

	close(pipe_fds[1]);
	total_read = 0;
	while ((nread = read(pipe_fds[0], io_buf, sizeof(io_buf))) > 0) {
		if (fwrite(io_buf, 1, (size_t)nread, tmp_out) != (size_t)nread)
			kbsh_exit(errno ? errno : 1);
		total_read += (size_t)nread;
	}
	close(pipe_fds[0]);
	if (nread < 0)
		kbsh_exit(errno ? errno : 1);

	if (waitpid(pid, &wait_status, 0) < 0)
		kbsh_exit(errno);

	if (fseek(tmp_out, 0, SEEK_SET) != 0)
		kbsh_exit(errno ? errno : 1);

	if (kbsh_arena_alloc(arena, total_read + 1, 1, &arena_buf) !=
	    KBSH_ARENA_SUCCESS)
		kbsh_exit(ENOMEM);

	if (total_read > 0 &&
	    fread(arena_buf, 1, total_read, tmp_out) != total_read) {
		kbsh_exit(errno ? errno : 1);
	}

	trimmed_size = total_read;
	while (trimmed_size > 0 && arena_buf[trimmed_size - 1] == '\n')
		trimmed_size--;
	arena_buf[trimmed_size] = '\0';
	*output = (char *)arena_buf;

	fclose(tmp_in);
	fclose(tmp_out);

	return kbsh_wait_status_code(wait_status);
}

static int kbsh_wait_status_code(int wait_status)
{
	if (WIFEXITED(wait_status))
		return WEXITSTATUS(wait_status);
	if (WIFSIGNALED(wait_status))
		return 128 + WTERMSIG(wait_status);
	return 1;
}
