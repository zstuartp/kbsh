/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

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
