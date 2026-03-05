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

#endif/*ENV_H*/
