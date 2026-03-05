/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

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
