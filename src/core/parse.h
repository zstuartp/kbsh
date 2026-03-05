/*
 * Parse input buffer.
 * Copyright (C) 2011 Zack Parsons <k3bacon@gmail.com>
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

#ifndef PARSE_H
#define PARSE_H

#include "core/arena.h"
#include "core/buffer.h"

enum kbsh_parse_result {
	KBSH_PARSE_OK,
	KBSH_PARSE_NEED_MORE,
	KBSH_PARSE_ERROR_MISSING_SQUOTE,
	KBSH_PARSE_ERROR_MISSING_DQUOTE,
	KBSH_PARSE_ERROR_MISSING_RBRACE,
	KBSH_PARSE_ERROR_UNEXPECTED_EOF
};

enum kbsh_parse_result kbsh_parse(struct Buffer *b,
				  struct kbsh_arena *arena,
				  int last_status);

#endif/*PARSE_H*/
