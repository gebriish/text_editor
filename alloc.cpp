#include "editor.h"

funcdef void
arena_make(Arena *arena, bytes buffer)
{
	arena->data = buffer;
	arena->used = 0;
}

funcdef void *
arena_alloc(Arena *arena, u64 size, u64 alignment)
{
	u64 pre = align_up_power_2(arena->used, alignment);
	u64 post = pre + size;

	if (post > arena->data.len) {
		return nullptr;
	}

	void *result = (void *) &arena->data.raw[pre];
	arena->used = post;
	return result;
}

funcdef void
arena_free(Arena *arena, u64 loc)
{
	assert(loc <= arena->used);
	arena->used = loc;
}
