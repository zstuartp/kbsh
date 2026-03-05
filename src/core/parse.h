/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

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
