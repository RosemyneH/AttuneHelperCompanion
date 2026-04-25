#include "ahc/ahc_arena.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define AHC_ARENA_MIN_ALIGN 16u

static size_t align_up(size_t n, size_t a)
{
    if (a == 0) {
        return n;
    }
    return (n + (a - 1)) & ~(a - 1u);
}

bool ahc_arena_init(AhcArena *arena, size_t initial_cap)
{
    if (!arena) {
        return false;
    }
    arena->data = NULL;
    arena->used = 0;
    arena->cap = 0;
    if (initial_cap == 0) {
        return true;
    }
    void *b = malloc(initial_cap);
    if (!b) {
        return false;
    }
    arena->data = b;
    arena->cap = initial_cap;
    return true;
}

void ahc_arena_free(AhcArena *arena)
{
    if (!arena) {
        return;
    }
    free(arena->data);
    arena->data = NULL;
    arena->used = 0;
    arena->cap = 0;
}

void *ahc_arena_alloc(AhcArena *arena, size_t size, size_t align)
{
    if (!arena) {
        return NULL;
    }
    if (size == 0) {
        return NULL;
    }
    size_t al = align == 0 ? 1u : align;
    if (al < AHC_ARENA_MIN_ALIGN) {
        al = AHC_ARENA_MIN_ALIGN;
    }
    size_t next = align_up(arena->used, al);
    if (next > arena->cap || size > arena->cap - next) {
        size_t need = next + size;
        size_t new_cap = arena->cap ? arena->cap : 64u;
        while (new_cap < need) {
            size_t doubled = new_cap * 2u;
            if (doubled < new_cap) {
                return NULL;
            }
            new_cap = doubled;
        }
        uint8_t *b = (uint8_t *)realloc(arena->data, new_cap);
        if (!b) {
            return NULL;
        }
        arena->data = b;
        arena->cap = new_cap;
    }
    next = align_up(arena->used, al);
    void *p = arena->data + next;
    arena->used = next + size;
    return p;
}

char *ahc_arena_strcpy(AhcArena *arena, const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s) + 1u;
    char *d = (char *)ahc_arena_alloc(arena, n, 1u);
    if (!d) {
        return NULL;
    }
    memcpy(d, s, n);
    return d;
}

bool ahc_arena_push_bytes(AhcArena *arena, const void *src, size_t len, char **out)
{
    if (!arena || (len > 0 && !src) || !out) {
        return false;
    }
    char *d = (char *)ahc_arena_alloc(arena, len, 1u);
    if (!d) {
        return false;
    }
    if (len > 0) {
        memcpy(d, src, len);
    }
    *out = d;
    return true;
}

size_t ahc_arena_used(const AhcArena *arena)
{
    return arena ? arena->used : 0u;
}

size_t ahc_arena_cap(const AhcArena *arena)
{
    return arena ? arena->cap : 0u;
}
