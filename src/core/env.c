/*
 * Manage environmental variables.
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

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "core/kbsh.h"
#include "core/env.h"

struct Env env;

static void kbsh_env_get_cwd_end(void);

void kbsh_env_exit(void)
{
	return;
}

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
