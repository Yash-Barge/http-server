#include <stddef.h>
#include <stdlib.h>
#include <stdalign.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>

#include "arena.h"

#define VOIDPTR_ADD(voidptr, x) ((void *) (((char *) (voidptr)) + x))
#define ALIGNED_PTR(ptr) VOIDPTR_ADD((ptr), ((~((uintptr_t) (ptr)) + 1) & (_Alignof(max_align_t) - 1)))

struct arena {
    struct arena *next;
    void *limit;
    void *avail;
};

static size_t private_size_align(const size_t size) {
    const size_t page_size = sysconf(_SC_PAGE_SIZE);
    const size_t first_page_avail = page_size - ((size_t) ceil(((float) sizeof(struct arena)) / _Alignof(max_align_t))) * _Alignof(max_align_t);

    if (size <= first_page_avail)
        return page_size;

    return (1 + (int) ceil(((float) (size - first_page_avail)) / page_size)) * page_size;
}

static struct arena *private_arena_create(const size_t size) {
    const size_t aligned_size = private_size_align(size);
    void *ptr = malloc(aligned_size);

    if (ptr == NULL)
        return NULL;

    *((struct arena *) ptr) = (struct arena) {
        .next = NULL,
        .limit = VOIDPTR_ADD(ptr, aligned_size),
        .avail = ALIGNED_PTR(VOIDPTR_ADD(ptr, sizeof(struct arena)))
    };

    return ptr;
}

struct arena *arena_new(void) {
    return private_arena_create(0);
}

void *arena_alloc(struct arena *a, const size_t size) {
    if (VOIDPTR_ADD(a->avail, size) > a->limit) {
        if (a->next == NULL) {
            if ((a->next = private_arena_create(size)) == NULL)
                return NULL;
        }

        return arena_alloc(a->next, size);
    }

    void *ptr = a->avail;
    a->avail = ALIGNED_PTR(VOIDPTR_ADD(ptr, size));
    
    return ptr;
}

void arena_clear(struct arena *a) {
    struct arena *tracker = a;

    while (tracker != NULL) {
        tracker->avail = ALIGNED_PTR(VOIDPTR_ADD(tracker, sizeof(struct arena)));
        tracker = tracker->next;
    }

    return;
}

void arena_free(struct arena **p_a) {
    struct arena *tracker = *p_a;

    while (tracker != NULL) {
        struct arena *temp = tracker->next;
        free(tracker);
        tracker = temp;
    }

    *p_a = NULL;

    return;
}
