#ifndef AHC_ARENA_H
#define AHC_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AhcArena {
    uint8_t *data;
    size_t used;
    size_t cap;
} AhcArena;

bool ahc_arena_init(AhcArena *arena, size_t initial_cap);
void ahc_arena_free(AhcArena *arena);

void *ahc_arena_alloc(AhcArena *arena, size_t size, size_t align);
char *ahc_arena_strcpy(AhcArena *arena, const char *s);
bool ahc_arena_push_bytes(AhcArena *arena, const void *src, size_t len, char **out);
size_t ahc_arena_used(const AhcArena *arena);
size_t ahc_arena_cap(const AhcArena *arena);

#endif
