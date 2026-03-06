/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <stdlib.h>

#include "builtin/builtin.h"
#include "core/kbsh.h"

int kbsh_builtin_exit(struct kbsh_cmd *cmd, struct kbsh_arena *arena)
{
	int status = 0;
	if (!cmd || cmd->argc < 2)
		goto end;
	status = atoi(cmd->argv[1]);
end:
	(void)arena;
	kbsh_exit(status);
	return 1;
}

struct Builtin bi_exit = { "exit", kbsh_builtin_exit };
