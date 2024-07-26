#ifndef H_SIZED_STR
#define H_SIZED_STR

#include <stdlib.h>

#define IS_WHITESPACE(c) ((c) == 0x09 || (c) == 0x0A || (c) == 0x0C || (c) == 0x0D || (c) == 0x20)

struct sized_str {
    char *ptr;
    size_t len;
};

int post_prefix_index(const struct sized_str str, const char *restrict prefix);
int is_same_string(struct sized_str str, const char *restrict str2);

#endif
