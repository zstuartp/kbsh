/* Copyright 2026 Zackary Parsons. Licensed under GPLv3. */

#include "arena.h"

static int kbsh_arena_align_offset(size_t offset, size_t alignment, size_t *out)
{
	size_t mask = 0;
	size_t aligned = 0;

	if (out == NULL || alignment == 0) {
		return KBSH_ARENA_ERROR_ALIGN_INVALID;
	}
	if (alignment & (alignment - 1)) {
		return KBSH_ARENA_ERROR_ALIGN_INVALID;
	}

	mask = alignment - 1;
	if (offset > ((size_t)-1) - mask) {
		return KBSH_ARENA_ERROR_OVERFLOW;
	}

	aligned = (offset + mask) & ~mask;
	*out = aligned;

	return KBSH_ARENA_SUCCESS;
}

int kbsh_arena_init(struct kbsh_arena *arena, unsigned char *pool, size_t size)
{
	if (arena == NULL) {
		return KBSH_ARENA_ERROR_ARENA_NULL;
	}
	if (pool == NULL) {
		return KBSH_ARENA_ERROR_POOL_NULL;
	}
	if (size == 0) {
		return KBSH_ARENA_ERROR_ALLOC_SIZE_INVALID;
	}

	arena->buffer = pool;
	arena->offset = 0;
	arena->size = size;
	arena->peak_fill = 0;

	return KBSH_ARENA_SUCCESS;
}

void kbsh_arena_destroy(struct kbsh_arena *arena)
{
	if (arena == NULL) {
		return;
	}

	arena->buffer = NULL;
	arena->offset = 0;
	arena->size = 0;
	arena->peak_fill = 0;
}

void kbsh_arena_reset(struct kbsh_arena *arena)
{
	if (arena == NULL) {
		return;
	}

	arena->offset = 0;
}

size_t kbsh_arena_mark(const struct kbsh_arena *arena)
{
	if (arena == NULL) {
		return 0;
	}

	return arena->offset;
}

int kbsh_arena_rewind(struct kbsh_arena *arena, size_t mark)
{
	if (arena == NULL) {
		return KBSH_ARENA_ERROR_ARENA_NULL;
	}
	if (arena->buffer == NULL || arena->size == 0) {
		return KBSH_ARENA_ERROR_ARENA_INVALID;
	}
	if (mark > arena->offset) {
		return KBSH_ARENA_ERROR_MARK_INVALID;
	}

	arena->offset = mark;

	return KBSH_ARENA_SUCCESS;
}

int kbsh_arena_alloc(struct kbsh_arena *arena,
		     size_t size,
		     size_t align,
		     unsigned char **out)
{
	size_t aligned_offset = 0;
	size_t next_offset = 0;
	int error = KBSH_ARENA_SUCCESS;

	/* Validate inputs. */
	if (arena == NULL) {
		return KBSH_ARENA_ERROR_ARENA_NULL;
	}
	if (arena->buffer == NULL || arena->size == 0) {
		return KBSH_ARENA_ERROR_ARENA_INVALID;
	}
	if (out == NULL) {
		return KBSH_ARENA_ERROR_OUTPTR_NULL;
	}
	if (size == 0) {
		return KBSH_ARENA_ERROR_ALLOC_SIZE_INVALID;
	}

	*out = NULL;

	/* Resolve effective alignment. */
	if (align == 0) {
		align = KBSH_ARENA_DEFAULT_ALIGN;
	}

	/* Compute aligned write position. */
	error = kbsh_arena_align_offset(arena->offset, align, &aligned_offset);
	if (error != KBSH_ARENA_SUCCESS) {
		return error;
	}

	/* Ensure allocation is representable and within pool bounds. */
	if (aligned_offset > arena->size) {
		return KBSH_ARENA_ERROR_OUT_OF_MEMORY;
	}
	if (size > arena->size - aligned_offset) {
		return KBSH_ARENA_ERROR_OUT_OF_MEMORY;
	}
	if (aligned_offset > ((size_t)-1) - size) {
		return KBSH_ARENA_ERROR_OVERFLOW;
	}

	next_offset = aligned_offset + size;

	/* Commit allocation. */
	*out = &(arena->buffer[aligned_offset]);
	arena->offset = next_offset;
	if (arena->offset > arena->peak_fill) {
		arena->peak_fill = arena->offset;
	}

	return KBSH_ARENA_SUCCESS;
}
