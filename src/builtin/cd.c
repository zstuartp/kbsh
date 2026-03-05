/*
 * Change directory.
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

#include <unistd.h>

#include "core/kbsh.h"
#include "core/buffer.h"
#include "core/env.h"
#include "builtin/builtin.h"

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

struct Builtin bi_cd = {
	"cd",
	kbsh_builtin_cd
};
