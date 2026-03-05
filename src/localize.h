/*
 * Localization
 * Copyright (C) 2011, 2012 Zack Parsons <parsons.zackary@gmail.com>
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

#ifndef LOCALIZE_H
#define LOCALIZE_H

#include <locale.h>

#if 0 /* Not yet */
#include "gettext.h"
#define _(str) gettext(str)
#define N_(str) gettext_noop(str)
#else
#define _(str) str
#define N_(str) str
#endif

#endif/*LOCALIZE_H*/
