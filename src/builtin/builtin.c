/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <string.h>

#include "builtin/builtin.h"
#include "core/kbsh.h"

int kbsh_find_builtin(struct kbsh_cmd *cmd, struct kbsh_arena *arena)
{
	if (!cmd)
		kbsh_exit(EINVAL);
	if (cmd->argc == 0 || !cmd->argv || !cmd->argv[0])
		goto found;

	if (!strcmp(cmd->argv[0], bi_cd.command)) {
		bi_cd.init(cmd, arena);
		goto found;
	} else if (!strcmp(cmd->argv[0], bi_echo.command)) {
		bi_echo.init(cmd, arena);
		goto found;
	} else if (!strcmp(cmd->argv[0], bi_printf.command)) {
		bi_printf.init(cmd, arena);
		goto found;
	} else if (!strcmp(cmd->argv[0], bi_exit.command)) {
		bi_exit.init(cmd, arena);
		goto found;
	}

	return 0;
found:
	return 1;
}
