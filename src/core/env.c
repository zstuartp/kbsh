/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "core/env.h"
#include "core/kbsh.h"

struct Env env;

static void kbsh_env_get_cwd_end(void);

void kbsh_env_exit(void) { return; }

void kbsh_env_init(void)
{
	if (!getcwd(env.cwd, sizeof(env.cwd)))
		kbsh_exit(errno);
	env.home = getenv("HOME");
	if (!env.home)
		kbsh_exit(errno);
	env.user = getenv("USER");
	if (!env.user)
		kbsh_exit(errno);
	kbsh_env_get_cwd_end();
}

void kbsh_env_update(void)
{
	if (!getcwd(env.cwd, sizeof(env.cwd)))
		kbsh_exit(errno);
	kbsh_env_get_cwd_end();
}

static void kbsh_env_get_cwd_end(void)
{
	char *cwd = NULL;
	char *sep = "/";
	size_t index = 0;
	if (!env.home)
		return;
	cwd = env.cwd;
	if (!strcmp(cwd, env.home)) {
		env.cwd_end = "~";
		return;
	}
	while (cwd[index] != '\0')
		index++;
	while (cwd[index] != *sep)
		index--;
	if (&cwd[index] == cwd && !cwd[index + 1])
		cwd = sep;
	else
		cwd = &cwd[index + 1];
	env.cwd_end = cwd;
}
