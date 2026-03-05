/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef ENV_H
#define ENV_H

#include <limits.h>

struct Env {
	char cwd[PATH_MAX + 1];
	char *cwd_end;
	char *home;
	char *user;
};

extern struct Env env;

void kbsh_env_exit(void);
void kbsh_env_init(void);
void kbsh_env_update(void);

#endif /*ENV_H*/
