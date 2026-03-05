/*
 * Signal handling.
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

#include <signal.h>

#include "core/kbsh.h"
#include "core/sig.h"

#define kbsh_sig_exit kbsh_exit

void kbsh_sig_init(void)
{
	signal(SIGHUP,  kbsh_sig_exit);/*terminal closed*/
	signal(SIGQUIT, kbsh_sig_exit);/*ctrl-\*/
	signal(SIGTERM, kbsh_sig_exit);/*system shutdown*/
}
