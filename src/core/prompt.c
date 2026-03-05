/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "core/env.h"
#include "core/kbsh.h"
#include "core/prompt.h"

struct Prompt prompt;

void kbsh_prompt_exit(void) { return; }

void kbsh_prompt_init(void)
{
	uid_t uid = getuid();

	prompt.root_ch = "# ";
	prompt.dflt_ch = "$ ";
	prompt.scnd_ch = "> ";

	if (!uid) /* root */
		prompt.crnt_ch = prompt.root_ch;
	else if (uid > 0) /* not root */
		prompt.crnt_ch = prompt.dflt_ch;
}
