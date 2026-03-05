/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <stdlib.h>

#include "builtin/builtin.h"
#include "core/buffer.h"
#include "core/kbsh.h"

int kbsh_builtin_exit(struct Buffer *b, struct kbsh_arena *arena)
{
	int status = 0;
	if (!b || b->word_used < 2)
		goto end;
	status = atoi(b->word[1]);
end:
	(void)arena;
	kbsh_exit(status);
	return 1;
}

struct Builtin bi_exit = { "exit", kbsh_builtin_exit };
