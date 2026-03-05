/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef PROMPT_H
#define PROMPT_H

struct Prompt {
	char *crnt_ch;
	char *dflt_ch;
	char *root_ch;
	char *scnd_ch;
};

extern struct Prompt prompt;

void kbsh_prompt_exit(void);
void kbsh_prompt_init(void);

#endif/*PROMPT_H*/
