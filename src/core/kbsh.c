/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>
#include <unistd.h>

#include "localize.h"

#include "builtin/builtin.h"
#include "core/arena.h"
#include "core/buffer.h"
#include "core/env.h"
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
	enum kbsh_state_id state_id;
	enum kbsh_event_id event_id;
	enum kbsh_run_mode_id run_mode_id;
	int last_command_status;
	size_t arena_mark;
	struct Buffer buffer;
};

static enum kbsh_event_id get_input(struct kbsh_state *,
				    FILE *,
				    FILE *,
				    struct kbsh_arena *);
static enum kbsh_event_id parse_input(struct kbsh_state *, struct kbsh_arena *);
static enum kbsh_event_id exec_cmd(struct kbsh_state *, struct kbsh_arena *);
static enum kbsh_event_id do_cleanup(struct kbsh_state *, struct kbsh_arena *);
static enum kbsh_state_id kbsh_transition(const struct kbsh_state *);

static int kbsh_exec(char **argums);
static void kbsh_fork(struct Buffer *b);
static char *kbsh_run_read_line(FILE *fp);

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
		old_len = strlen(state->buffer.full);
		len = strlen(line);
		if (kbsh_arena_alloc(arena, old_len + len + 1, 1, &arena_buf) !=
		    KBSH_ARENA_SUCCESS)
			kbsh_exit(1);
		memcpy(arena_buf, state->buffer.full, old_len);
		memcpy(arena_buf + old_len, line, len + 1);
		state->buffer.full = (char *)arena_buf;
		state->buffer.full_size = old_len + len + 1;
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
	state->buffer.full = (char *)arena_buf;
	state->buffer.full_size = len + 1;
	return KBSH_EVENT_OKAY;
}

static enum kbsh_event_id parse_input(struct kbsh_state *state,
				      struct kbsh_arena *arena)
{
	enum kbsh_parse_result result;

	result = kbsh_parse(&state->buffer, arena, state->last_command_status);
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
	kbsh_main(&state->buffer, arena);
	state->last_command_status = 0;
	if (state->run_mode_id == KBSH_RUN_MODE_INTERACTIVE)
		kbsh_input_save_history();
	return KBSH_EVENT_OKAY;
}

static enum kbsh_event_id do_cleanup(struct kbsh_state *state,
				     struct kbsh_arena *arena)
{
	(void)kbsh_arena_rewind(arena, state->arena_mark);
	memset(&state->buffer, 0, sizeof(state->buffer));
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

void kbsh_main(struct Buffer *b, struct kbsh_arena *arena)
{
	size_t i;

	if (b->word_used > 0) {
		for (i = 0; i < b->word_used; i++) {
			if (!is_assignment(b->word[i]))
				break;
		}
		if (i == b->word_used) {
			for (i = 0; i < b->word_used; i++)
				do_assignment(b->word[i]);
			return;
		}
	}

	if (!kbsh_find_builtin(b, arena))
		kbsh_fork(b);
}

static int kbsh_exec(char **argums)
{
	int err = 0;

	err = execvp(argums[0], argums);
	if (err) {
		fprintf(stderr, "%s: ", program_name);
		perror(argums[0]);
	}
	return err;
}

static void kbsh_fork(struct Buffer *b)
{
	pid_t pid = fork();

	if (!pid)
		_exit(kbsh_exec(b->word));
	else if (pid > 0)
		wait(NULL);
	if (pid < 0)
		kbsh_exit(errno);
}

static char *kbsh_run_read_line(FILE *fp)
{
	static char s_line_buf[4096];

	if (!fgets(s_line_buf, sizeof(s_line_buf), fp))
		return NULL;
	return s_line_buf;
}
