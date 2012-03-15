/*
 * Manage input buffer.
 * Copyright (C) 2011, 2012 Zack Parsons <k3bacon@gmail.com>
 *
 * This file is part of kbsh.
 *
 * Kbsh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include <errno.h>

#include "core/kbsh.h"
#include "core/buffer.h"

struct Buffer *kbsh_buffer_add_bytes(struct Buffer *b, size_t bytes)
{
	if (!b || !b->full || !b->pars)
		goto err;
	b->pars_size += bytes;
	b->full_size += bytes;

	b->full = realloc(b->full, (sizeof(*b->full) * (b->full_size)));
	if (!b->full)
		goto err;
	b->pars = realloc(b->pars, (sizeof(*b->pars) * (b->pars_size)));
	if (!b->pars)
		goto err;

	return b;
err:
	return NULL;
}

void kbsh_buffer_reset(struct Buffer *b)
{
	if (b->full)
		free(b->full);
	if (b->pars)
		free(b->pars);
	if (b->word)
		free(b->word);
	b->full = NULL;
	b->full_size = 0;
	b->full_used = 0;
	b->pars = NULL;
	b->pars_size = 0;
	b->pars_used = 0;
	b->word = NULL;
	b->word_size = 0;
	b->word_used = 0;
}
