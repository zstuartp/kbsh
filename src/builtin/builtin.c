/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <string.h>

#include "builtin/builtin.h"
#include "core/buffer.h"
#include "core/kbsh.h"

int kbsh_find_builtin(struct Buffer *b, struct kbsh_arena *arena)
{
	if (!b)
		kbsh_exit(EINVAL);
	/* find builtin commands */
	if (!b->word || !*b->word) /* empty input */
		goto found;

	if (!strcmp(b->word[0], bi_cd.command)) {
		bi_cd.init(b, arena);
		goto found;
	} else if (!strcmp(b->word[0], bi_echo.command)) {
		bi_echo.init(b, arena);
		goto found;
	} else if (!strcmp(b->word[0], bi_printf.command)) {
		bi_printf.init(b, arena);
		goto found;
	} else if (!strcmp(b->word[0], bi_exit.command)) {
		bi_exit.init(b, arena);
		goto found;
	}

	return 0; /* Continue and find external commands */
found:
	return 1; /* Builtin command found; Don't continue */
}
