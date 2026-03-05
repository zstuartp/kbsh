/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "core/env.h"
#include "core/input.h"
#include "core/kbsh.h"
#include "core/prompt.h"

static char history_fname[PATH_MAX + 32];

static void kbsh_create_histfname(void);

void kbsh_input_exit(void) { kbsh_prompt_exit(); }

void kbsh_input_init(void)
{
	kbsh_clean = kbsh_input_exit;
	kbsh_prompt_init();
	kbsh_create_histfname();
	rl_outstream = stderr;
	read_history(history_fname);
	rl_bind_key('\t', rl_complete);
}

void kbsh_input_save_history(void) { write_history(history_fname); }

/*
 * Read one line via readline.  Returns a malloc'd string that the caller
 * must free, or NULL on EOF.  Adds non-empty lines to readline history.
 * The returned string includes a trailing newline to match kbsh_run_read_line.
 */
char *kbsh_input_readline(const char *prompt_str)
{
	char *line = NULL;
	size_t line_size;

	line = readline(prompt_str);

	if (!line)
		return NULL;

	if (*line)
		add_history(line);

	/* readline omits the newline; add it to match kbsh_run_read_line */
	line_size = strlen(line) + 2;
	line = realloc(line, line_size);
	if (!line)
		kbsh_exit(errno);
	strcat(line, "\n");

	return line;
}

static void kbsh_create_histfname(void)
{
	if (!env.home)
		return;
	snprintf(
	    history_fname, sizeof(history_fname), "%s/.kbsh_history", env.home);
}
