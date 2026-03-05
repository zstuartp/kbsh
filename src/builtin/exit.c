/*
 * Buitin command: exit
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

#include <stdlib.h>

#include "core/kbsh.h"
#include "core/buffer.h"
#include "builtin/builtin.h"

int kbsh_builtin_exit(struct Buffer *b)
{
	int status = 0;
	if (!b || b->word_used < 2)
		goto end;
	status = atoi(b->word[1]);
end:
	kbsh_exit(status);
	return 1;
}

struct Builtin bi_exit = {
	"exit",
	kbsh_builtin_exit
};
