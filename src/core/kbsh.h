/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef KBSH_H
#define KBSH_H

#include <stdio.h>

#include "core/arena.h"
#include "core/buffer.h"
#include "core/pipeline.h"

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

void kbsh_main(struct kbsh_pipeline *pl, struct kbsh_arena *arena);

int kbsh_capture_command_output(const char *command,
				struct kbsh_arena *arena,
				char **output);

#endif /*KBSH_H*/
