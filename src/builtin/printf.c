/* Copyright 2026 Zackary Parsons. Licensed under GPLv3. */

#include <config.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin/builtin.h"
#include "core/arena.h"
#include "core/buffer.h"
#include "core/kbsh.h"

/*
 * Expand C-style escape sequences in src into dst (already allocated).
 * Returns the number of bytes written (excluding NUL).
 */
static size_t expand_escapes(const char *src, char *dst, size_t dst_size)
{
	size_t out = 0;
	const char *p = src;

	while (*p && out + 1 < dst_size) {
		if (*p != '\\') {
			dst[out++] = *p++;
			continue;
		}
		p++; /* skip backslash */
		switch (*p) {
		case 'n':  dst[out++] = '\n'; p++; break;
		case 't':  dst[out++] = '\t'; p++; break;
		case 'r':  dst[out++] = '\r'; p++; break;
		case '\\': dst[out++] = '\\'; p++; break;
		case '\'': dst[out++] = '\''; p++; break;
		case '"':  dst[out++] = '"';  p++; break;
		case '0':  dst[out++] = '\0'; p++; break;
		default:
			/* unrecognised — emit backslash and the char */
			if (out + 2 < dst_size) {
				dst[out++] = '\\';
				dst[out++] = *p++;
			}
			break;
		}
	}
	dst[out] = '\0';
	return out;
}

/*
 * kbsh_builtin_printf — minimal POSIX-subset printf builtin.
 *
 * Uses the arena to allocate a work buffer for the expanded format string
 * and each formatted argument, so no heap allocation is needed.
 *
 * Supported conversions: %s  %d  %i  %u  %f  %g  %%
 * Escape sequences in the format string: \n \t \r \\ \' \" \0
 */
int kbsh_builtin_printf(struct Buffer *b, struct kbsh_arena *arena)
{
	const char *fmt = NULL;
	size_t arg_idx   = 2; /* b->word[0]=printf, b->word[1]=format */
	unsigned char *arena_buf = NULL;
	size_t buf_size  = 0;
	char *work       = NULL;
	size_t work_size = 0;
	const char *p    = NULL;
	size_t mark      = 0;

	if (!b)
		kbsh_exit(EINVAL);

	if (b->word_used < 2 || !b->word[1]) {
		fputs("printf: missing format string\n", stderr);
		return 1;
	}

	fmt = b->word[1];

	/*
	 * Allocate a work buffer from the arena.  We need enough room for the
	 * expanded format string plus any individual numeric conversions.
	 * Allocate min(available, 64K) — generous for any reasonable printf.
	 */
	buf_size  = 65536;
	mark      = kbsh_arena_mark(arena);
	if (kbsh_arena_alloc(arena, buf_size, 1, &arena_buf)
	    != KBSH_ARENA_SUCCESS) {
		fputs("printf: out of arena memory\n", stderr);
		return 1;
	}
	work      = (char *)arena_buf;
	work_size = buf_size;

	/* Expand escape sequences in the format string into work buffer. */
	expand_escapes(fmt, work, work_size);

	p = work;
	while (*p) {
		if (*p != '%') {
			putchar(*p++);
			continue;
		}
		p++; /* skip % */

		switch (*p) {
		case '%':
			putchar('%');
			p++;
			break;

		case 's': {
			const char *arg = (arg_idx < b->word_used
					   && b->word[arg_idx])
					  ? b->word[arg_idx++] : "";
			fputs(arg, stdout);
			p++;
			break;
		}

		case 'd':
		case 'i': {
			const char *arg = (arg_idx < b->word_used
					   && b->word[arg_idx])
					  ? b->word[arg_idx++] : "0";
			printf("%d", atoi(arg));
			p++;
			break;
		}

		case 'u': {
			const char *arg = (arg_idx < b->word_used
					   && b->word[arg_idx])
					  ? b->word[arg_idx++] : "0";
			printf("%u", (unsigned int)strtoul(arg, NULL, 10));
			p++;
			break;
		}

		case 'f': {
			const char *arg = (arg_idx < b->word_used
					   && b->word[arg_idx])
					  ? b->word[arg_idx++] : "0";
			printf("%f", strtod(arg, NULL));
			p++;
			break;
		}

		case 'g': {
			const char *arg = (arg_idx < b->word_used
					   && b->word[arg_idx])
					  ? b->word[arg_idx++] : "0";
			printf("%g", strtod(arg, NULL));
			p++;
			break;
		}

		default:
			/* unrecognised conversion — emit literally */
			putchar('%');
			putchar(*p++);
			break;
		}
	}

	/* Release the arena work buffer — it's within the command's mark. */
	(void)kbsh_arena_rewind(arena, mark);

	return 0;
}

struct Builtin bi_printf = { "printf", kbsh_builtin_printf };
