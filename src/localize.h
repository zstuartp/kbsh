/* Copyright 2011, 2012, 2026 Zackary Parsons. Licensed under GPLv3. */

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

#endif /*LOCALIZE_H*/
