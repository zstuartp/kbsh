/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "builtin/builtin.h"
#include "core/env.h"
#include "core/kbsh.h"

int kbsh_builtin_cd(struct kbsh_cmd *cmd, struct kbsh_arena *arena)
{
	static char cd_path[PATH_MAX + 1];
	int up;
	size_t remaining;

	(void)arena;
	if (!cmd)
		kbsh_exit(EINVAL);

	if (cmd->argc == 0 || !cmd->argv[0])
		goto end;
	if (cmd->argc < 2) {
		if (chdir(env.home))
			perror("kbsh: cd");
		else
			goto done;
		goto end;
	}

	up = 1;
	cd_path[0] = '\0';
	remaining = sizeof(cd_path);

	while (cmd->argv[up]) {
		if (cd_path[0] != '\0') {
			strncat(cd_path, " ", remaining - 1);
			remaining -= 1;
		}
		strncat(cd_path, cmd->argv[up], remaining - 1);
		remaining -= strlen(cmd->argv[up]);
		up++;
	}

	if (chdir(cd_path))
		perror("kbsh: cd");
	else {
	done:
		kbsh_env_update();
		if (setenv("PWD", env.cwd, 1))
			perror("kbsh");
	}
end:
	return 0;
}

struct Builtin bi_cd = { "cd", kbsh_builtin_cd };
