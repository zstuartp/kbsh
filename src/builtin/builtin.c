/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <string.h>

#include "builtin/builtin.h"
#include "core/kbsh.h"

int kbsh_find_builtin(struct kbsh_cmd *cmd,
		      struct kbsh_arena *arena,
		      int *status)
{
	if (!cmd || !status)
		kbsh_exit(EINVAL);
	*status = 0;
	if (cmd->argc == 0 || !cmd->argv || !cmd->argv[0])
		return 1;

	if (!strcmp(cmd->argv[0], bi_cd.command)) {
		*status = bi_cd.init(cmd, arena);
		return 1;
	} else if (!strcmp(cmd->argv[0], bi_echo.command)) {
		*status = bi_echo.init(cmd, arena);
		return 1;
	} else if (!strcmp(cmd->argv[0], bi_printf.command)) {
		*status = bi_printf.init(cmd, arena);
		return 1;
	} else if (!strcmp(cmd->argv[0], bi_exit.command)) {
		*status = bi_exit.init(cmd, arena);
		return 1;
	}

	return 0;
}
