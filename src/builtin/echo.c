/* Copyright 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <stdio.h>

#include "builtin/builtin.h"
#include "core/buffer.h"
#include "core/kbsh.h"

int kbsh_builtin_echo(struct Buffer *b, struct kbsh_arena *arena)
{
	size_t i = 1;
	int newline = 1;

	(void)arena;
	if (!b)
		kbsh_exit(EINVAL);

	/* -n suppresses the trailing newline */
	if (b->word_used > 1 && b->word[1] && b->word[1][0] == '-' &&
	    b->word[1][1] == 'n' && b->word[1][2] == '\0') {
		newline = 0;
		i = 2;
	}

	for (; i < b->word_used && b->word[i]; i++) {
		if (i > (newline ? 1u : 2u))
			putchar(' ');
		fputs(b->word[i], stdout);
	}

	if (newline)
		putchar('\n');

	return 0;
}

struct Builtin bi_echo = { "echo", kbsh_builtin_echo };
