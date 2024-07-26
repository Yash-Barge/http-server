#ifndef H_LIB
#define H_LIB

#include "arena.h"
#include "sized_str.h"

void error_exit(const char *err_msg);
void print_to_log(const char *restrict fmt, ...);
char dir_or_file(const struct sized_str path, struct arena *arena);
struct sized_str read_file(const struct sized_str path, struct arena *arena);
struct sized_str validate_path(struct sized_str path, struct arena *arena);
int gzip_compress(char *restrict out_buf, struct sized_str str);
char *set_err_500(char *err_prefix, struct arena *arena);

#endif
