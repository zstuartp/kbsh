/* Copyright 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <stdio.h>

#include "builtin/builtin.h"
#include "core/kbsh.h"

int kbsh_builtin_echo(struct kbsh_cmd *cmd, struct kbsh_arena *arena)
{
	int i = 1;
	int newline = 1;

	(void)arena;
	if (!cmd)
		kbsh_exit(EINVAL);

	if (cmd->argc > 1 && cmd->argv[1] && cmd->argv[1][0] == '-' &&
	    cmd->argv[1][1] == 'n' && cmd->argv[1][2] == '\0') {
		newline = 0;
		i = 2;
	}

	for (; i < cmd->argc && cmd->argv[i]; i++) {
		if (i > (newline ? 1 : 2))
			putchar(' ');
		fputs(cmd->argv[i], stdout);
	}

	if (newline)
		putchar('\n');

	return 0;
}

struct Builtin bi_echo = { "echo", kbsh_builtin_echo };
