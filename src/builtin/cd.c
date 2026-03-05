/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "builtin/builtin.h"
#include "core/buffer.h"
#include "core/env.h"
#include "core/kbsh.h"

int kbsh_builtin_cd(struct Buffer *b)
{
	static char cd_path[PATH_MAX + 1];
	size_t up;
	size_t remaining;

	if (!b)
		kbsh_exit(EINVAL);

	if (!b->word_used || !b->word[0])
		goto end;
	if (b->word_used < 2) {
		if (chdir(env.home))
			perror("kbsh: cd");
		else
			goto done;
		goto end;
	}

	up = 1;
	cd_path[0] = '\0';
	remaining = sizeof(cd_path);

	while (b->word[up]) {
		if (cd_path[0] != '\0') {
			strncat(cd_path, " ", remaining - 1);
			remaining -= 1;
		}
		strncat(cd_path, b->word[up], remaining - 1);
		remaining -= strlen(b->word[up]);
		up++;
	}

	if (chdir(cd_path))
		perror("kbsh: cd");
	else {
	done:
		if (!getcwd(env.cwd, sizeof(env.cwd)))
			kbsh_exit(errno);
		if (setenv("PWD", env.cwd, 1))
			perror("kbsh");
	}
end:
	return 0;
}

struct Builtin bi_cd = { "cd", kbsh_builtin_cd };
