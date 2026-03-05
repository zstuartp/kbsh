/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef COMMAND_H
#define COMMAND_H

#include "core/arena.h"
#include "core/buffer.h"

struct Builtin {
	char *command;
	int (*init)(struct Buffer *b, struct kbsh_arena *arena);
};

int kbsh_find_builtin(struct Buffer *b, struct kbsh_arena *arena);

extern struct Builtin bi_cd;
extern struct Builtin bi_echo;
extern struct Builtin bi_exit;
extern struct Builtin bi_printf;

#endif /*COMMAND_H*/
