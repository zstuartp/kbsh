/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef KBSH_PIPELINE_H
#define KBSH_PIPELINE_H

#include "core/arena.h"

enum kbsh_redir_type {
	KBSH_REDIR_IN = 0, /* <    */
	KBSH_REDIR_OUT,	   /* >    */
	KBSH_REDIR_APPEND, /* >>   */
	KBSH_REDIR_ERR,	   /* 2>   */
	KBSH_REDIR_ERR_OUT /* 2>&1 */
};

#define KBSH_CMD_REDIR_MAX 8
#define KBSH_PIPELINE_MAX 16

struct kbsh_redir {
	enum kbsh_redir_type type;
	char *target; /* filename; NULL for KBSH_REDIR_ERR_OUT */
};

struct kbsh_cmd {
	char **argv;
	int argc;
	struct kbsh_redir redirs[KBSH_CMD_REDIR_MAX];
	int nredirs;
};

struct kbsh_pipeline {
	struct kbsh_cmd cmds[KBSH_PIPELINE_MAX];
	int ncmds;
	int expansion_status;
};

#endif /* KBSH_PIPELINE_H */
