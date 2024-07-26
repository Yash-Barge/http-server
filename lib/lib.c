#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>

#include <zlib.h>

#include "lib.h"

static const char filedir[] = "serve";
extern __thread char *g_err_500_msg;

void error_exit(const char *err_msg) {
    perror(err_msg);
    exit(EXIT_FAILURE);
}

void print_to_log(const char *restrict fmt, ...) {
    const time_t rawtime = time(NULL);
    const struct tm *timeinfo = localtime(&rawtime);

    flockfile(stdout);

    printf("[\033[2m%02d/%02d/%02d %02d:%02d:%02d\033[0m] ",
        timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year - 100,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec
    );

    va_list arg;
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);

    printf("\n");

    funlockfile(stdout);

    return;
}

char dir_or_file(const struct sized_str path, struct arena *arena) {
    char c_path[strlen(filedir)+path.len+1];
    memcpy(c_path, filedir, strlen(filedir));
    memcpy(c_path+strlen(filedir), path.ptr, path.len);
    c_path[strlen(filedir)+path.len] = '\0';

    struct stat st_buf;
    const int stat_retval = stat(c_path, &st_buf);

    if (stat_retval == -1) {
        if (errno == ENOENT || errno == ENOTDIR)
            return '\0';
        set_err_500("unexpected error while locating file", arena);
        return 'e';
    }

    if (S_ISDIR(st_buf.st_mode))
        return 'd';
    
    if (S_ISREG(st_buf.st_mode))
        return 'f';

    set_err_500("given path exists, but is neither file nor directory!", arena);
    return 'e';
}

struct sized_str read_file(const struct sized_str path, struct arena *arena) {
    char complete_filepath[strlen(filedir)+path.len+1];
    memcpy(complete_filepath, filedir, strlen(filedir));
    memcpy(complete_filepath+strlen(filedir), path.ptr, path.len);
    complete_filepath[strlen(filedir)+path.len] = '\0';

    FILE *f = fopen(complete_filepath, "rb");

    if (!f) {
        if (errno != ENOENT && errno != ENOTDIR)
            set_err_500("file exists, but failed to open", arena);
        return (struct sized_str) { 0 };
    }

    fseek(f, 0, SEEK_END);
    const int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);


    struct sized_str retval = { .ptr = arena_alloc(arena, file_size), .len = file_size };

    const int bytes_read = fread(retval.ptr, 1, file_size, f);

    if (bytes_read != file_size) {
        set_err_500("failure to read file", arena);
        return (struct sized_str) { 0 };
    }

    return retval;
}

struct sized_str validate_path(struct sized_str path, struct arena *arena) {
    if (!path.len)
        return (struct sized_str) { 0 };

    char *temp_buf = arena_alloc(arena, path.len+1); // TODO: scratch?
    memcpy(temp_buf, path.ptr, path.len);
    temp_buf[path.len] = '\0';

    char **tok_stack = arena_alloc(arena, sizeof(*tok_stack) * path.len);
    memset(tok_stack, 0, sizeof(*tok_stack) * path.len);
    int valid_tok_count = 0;

    char *saveptr;

    int total_tok_count;
    for (total_tok_count = 0; ; temp_buf = NULL, total_tok_count++) {
        char *tok = strtok_r(temp_buf, "/", &saveptr);

        if (!tok)
            break;

        int is_dir_up = !strcmp(tok, "..");

        if (!strcmp(tok, "."))
            continue;

        if (is_dir_up) {
            if (!valid_tok_count)
                return (struct sized_str) { 0 };

            tok_stack[--valid_tok_count] = NULL;
        } else
            tok_stack[valid_tok_count++] = tok;
    }
    // validated

    if (total_tok_count == valid_tok_count) // no `..` present in url
        return path;

    if (!valid_tok_count)
        return (struct sized_str) { .ptr = "/", .len = 1 };

    // rebuild
    const int new_length = path.len - 3*(total_tok_count - valid_tok_count);
    char *new_path = arena_alloc(arena, new_length);

    int offset = 0;
    for (int index = 0; offset < new_length && index < valid_tok_count; offset += strlen(tok_stack[index++])) {
        new_path[offset++] = '/';
        memcpy(new_path+offset, tok_stack[index], strlen(tok_stack[index]));
    }

    if (path.ptr[path.len-1] == '/') // preserve trailing slash
        new_path[offset++] = '/';

    return (struct sized_str) { .ptr = new_path, .len = offset };
}

int gzip_compress(char *restrict out_buf, struct sized_str str) {
	z_stream zs = { .zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL,
		.avail_in = str.len, .next_in = (Bytef *) str.ptr,
		.avail_out = str.len, .next_out = (Bytef *) out_buf
	};

	// TODO: error checking???
	deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	deflate(&zs, Z_FINISH);
	deflateEnd(&zs);

	return zs.total_out; // success
}

char *set_err_500(char *err_prefix, struct arena *arena) {
    int errnum = errno;

    char err_buf[1024];
    strerror_r(errnum, err_buf, 1024);

    const int length = snprintf(NULL, 0, "%s: [%d] %s", err_prefix, errnum, err_buf);

    g_err_500_msg = arena_alloc(arena, length+1);
    snprintf(g_err_500_msg, length+1, "%s: [%d] %s", err_prefix, errnum, err_buf);

    return g_err_500_msg;
}
