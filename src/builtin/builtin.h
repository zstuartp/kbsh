/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef COMMAND_H
#define COMMAND_H

#include "core/buffer.h"

struct Builtin {
	char *command;
	int (*init)(struct Buffer *b);
};

int kbsh_find_builtin(struct Buffer *b);

extern struct Builtin bi_cd;
extern struct Builtin bi_exit;

#endif /*COMMAND_H*/
