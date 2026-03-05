/*
 * Kbsh core.
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

#ifndef KBSH_H
#define KBSH_H

#include <stdio.h>

#include "core/buffer.h"

extern char *program_name;
extern void (*kbsh_clean)(void);
extern char **kbsh_positional_params;
extern int kbsh_positional_param_count;

enum kbsh_run_mode_id {
	KBSH_RUN_MODE_INTERACTIVE,
	KBSH_RUN_MODE_NONINTERACTIVE
};

void kbsh_init(void);

void kbsh_exit(int exit_status);

int kbsh_run(enum kbsh_run_mode_id mode, FILE *in, FILE *out);

void kbsh_main(struct Buffer *buffer);

#endif/*KBSH_H*/
