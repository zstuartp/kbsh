/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <limits.h>
#include <stdio.h>

#include "core/env.h"
#include "core/history.h"
#include "core/input.h"
#include "core/kbsh.h"
#include "core/line.h"
#include "core/prompt.h"

static char history_fname[PATH_MAX + 32];

void kbsh_input_exit(void)
{
	kbsh_line_exit();
	kbsh_prompt_exit();
}

void kbsh_input_init(void)
{
	kbsh_clean = kbsh_input_exit;
	kbsh_line_init();
	kbsh_prompt_init();
	if (env.home) {
		snprintf(history_fname,
			 sizeof(history_fname),
			 "%s/.kbsh_history",
			 env.home);
		kbsh_history_load(history_fname);
	}
}

void kbsh_input_save_history(void)
{
	if (history_fname[0])
		kbsh_history_save(history_fname);
}

char *kbsh_input_readline(const char *prompt_str)
{
	return kbsh_line_read(prompt_str);
}
