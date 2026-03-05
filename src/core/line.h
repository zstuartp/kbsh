/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef LINE_H
#define LINE_H

void kbsh_line_init(void);
void kbsh_line_exit(void);
char *kbsh_line_read(const char *prompt);

#endif /* LINE_H */
