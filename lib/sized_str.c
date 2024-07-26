#include <string.h>
#include <strings.h>

#include "sized_str.h"

int post_prefix_index(const struct sized_str str, const char *restrict prefix) {
    unsigned int offset = 0;
    const size_t str_len = strlen(prefix);

    while (IS_WHITESPACE(str.ptr[offset]))
        offset++;

	if ((str.len-offset) >= str_len && !strncasecmp(str.ptr+offset, prefix, str_len))
        return offset+str_len;

    return -1;
}

int is_same_string(struct sized_str str, const char *restrict str2) {
	return (str.len == strlen(str2)) && !memcmp(str.ptr, str2, str.len);
}
