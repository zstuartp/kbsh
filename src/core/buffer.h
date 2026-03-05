/* Copyright 2011, 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef BUFFER_H
#define BUFFER_H

#include <string.h>

struct Buffer {
	char *full;		/* full, raw buffer */
	size_t full_size;	/* size of malloc for .full in bytes */
	size_t full_used;	/* number bytes used in .full */
	char *pars;		/* after parsing, args separated by 0x1d */
	size_t pars_size;	/* size of malloc for .pars in bytes */
	size_t pars_used;	/* number of bytes used in .pars */
	char **word;		/* points to each argument in .pars */
	size_t word_size;	/* size of malloc for .word in (char *)s */
	size_t word_used;	/* number of (char *)s used in .word */
};


#endif/*BUFFER_H*/
