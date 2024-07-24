#ifndef H_ARENA
#define H_ARENA

#include <stddef.h>

struct arena;

struct arena *arena_new(void);
void *arena_alloc(struct arena *arena, const size_t size);
void arena_clear(struct arena *arena);
void arena_free(struct arena **p_arena);

#endif
