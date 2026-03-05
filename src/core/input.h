/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef INPUT_H
#define INPUT_H

void kbsh_input_exit(void);
void kbsh_input_init(void);
void kbsh_input_save_history(void);
char *kbsh_input_readline(const char *prompt_str);

#endif/*INPUT_H*/
