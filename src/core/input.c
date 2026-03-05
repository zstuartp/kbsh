/*
 * Manage user input.
 * Copyright (C) 2011 Zack Parsons <parsons.zackary@gmail.com>
 *
 * This file is part of kbsh.
 *
 * Kbsh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Kbsh is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with kbsh.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "core/kbsh.h"
#include "core/input.h"
#include "core/env.h"
#include "core/prompt.h"

static char history_fname[PATH_MAX + 32];

static void kbsh_create_histfname(void);

void kbsh_input_exit(void)
{
	kbsh_prompt_exit();
}

void kbsh_input_init(void)
{
	kbsh_clean = kbsh_input_exit;
	kbsh_prompt_init();
	kbsh_create_histfname();
	rl_outstream = stderr;
	read_history(history_fname);
	rl_bind_key('\t', rl_complete);
}

void kbsh_input_save_history(void)
{
	write_history(history_fname);
}

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
	snprintf(history_fname, sizeof(history_fname),
		 "%s/.kbsh_history", env.home);
}
