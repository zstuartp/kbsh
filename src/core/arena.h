/* Copyright 2026 Zackary Parsons. Licensed under GPLv3. */

#ifndef KBSH_ARENA_H
#define KBSH_ARENA_H

#include <stddef.h>

#define KBSH_POOL_SIZE (256u * 1024u)
#define KBSH_ARENA_DEFAULT_ALIGN (sizeof(void *))

enum kbsh_arena_return_codes {
	KBSH_ARENA_SUCCESS=0,
	KBSH_ARENA_ERROR_ARENA_NULL,
	KBSH_ARENA_ERROR_POOL_NULL,
	KBSH_ARENA_ERROR_OUTPTR_NULL,
	KBSH_ARENA_ERROR_ARENA_INVALID,
	KBSH_ARENA_ERROR_MARK_INVALID,
	KBSH_ARENA_ERROR_ALIGN_INVALID,
	KBSH_ARENA_ERROR_OVERFLOW,
	KBSH_ARENA_ERROR_OUT_OF_MEMORY,
	KBSH_ARENA_ERROR_ALLOC_SIZE_INVALID
};

struct kbsh_arena {
	unsigned char *buffer;	/* stack-based memory buffer */
	size_t size;		/* buffer size, in bytes */
	size_t offset;		/* current write head */
	size_t peak_fill;	/* maximum offset reached */
};

int kbsh_arena_init(struct kbsh_arena *arena,
		    unsigned char *pool,
		    size_t size);

void kbsh_arena_destroy(struct kbsh_arena *arena);

void kbsh_arena_reset(struct kbsh_arena *arena);

size_t kbsh_arena_mark(const struct kbsh_arena *arena);

int kbsh_arena_rewind(struct kbsh_arena *arena, size_t mark);

int kbsh_arena_alloc(struct kbsh_arena *arena,
		     size_t size,
		     size_t align,
		     unsigned char **out);

#endif/*KBSH_ARENA_H*/
