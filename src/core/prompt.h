/*
 * Manage input prompt.
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
