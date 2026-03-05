/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef HISTORY_H
#define HISTORY_H

void kbsh_history_add(const char *line);
const char *kbsh_history_get(int index);
int kbsh_history_count(void);
void kbsh_history_load(const char *path);
void kbsh_history_save(const char *path);

#endif /* HISTORY_H */
