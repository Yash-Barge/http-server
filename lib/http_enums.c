#define H_USE_INTERNAL_HTTP_ENUMS

#include <string.h>

#include "http_enums.h"

const char *http_status_codes_str[] = {
    [200] = "OK",
    [204] = "No Content",
    [301] = "Moved Permanently",
    [308] = "Permanent Redirect",
    [400] = "Bad Request",
    [404] = "Not Found",
    [405] = "Method Not Allowed",
    [500] = "Internal Server Error"
};

const char *http_methods_str[] = { FOREACH_HTTP_METHOD(GENERATE_STRING) };

enum http_methods method_enumify(const struct sized_str str) {
	for (int i = 0; i < METHOD_COUNT; i++)
		if (!strncmp(str.ptr, http_methods_str[i], str.len))
			return i;

	return -1;
}

const char *http_headers_str[] = { FOREACH_HTTP_HEADER(HEADER_STRINGIFY) };

const char *http_content_type_str[] = { FOREACH_HTTP_CONTENT_TYPE(HTTP_CONTENT_TYPE_STRINGIFY) };

#define RET_IF(str, retval) do { if (is_same_string(file_extension, str)) return retval; } while (0)

// TODO: image/svg+xml not supported (only svg MIME type)
enum http_content_type get_file_type(const struct sized_str path) {
    int i;
    for (i = path.len-1; i >= 0; i--)
        if (path.ptr[i] == '.')
            break;
    
    if (i == -1) // no file extension, fallback to octet stream
        return http_content_type_application_octet_stream;
    
    const struct sized_str file_extension = { .ptr = path.ptr + i + 1, .len = path.len - i - 1 };

    RET_IF("avif", http_content_type_image_avif);
    RET_IF("avifs", http_content_type_image_avif);
    RET_IF("bmp", http_content_type_image_bmp);
    RET_IF("gif", http_content_type_image_gif);
    RET_IF("jpg", http_content_type_image_jpeg);
    RET_IF("jpeg", http_content_type_image_jpeg);
    RET_IF("png", http_content_type_image_png);
    RET_IF("ico", http_content_type_image_x_icon);
    RET_IF("webp", http_content_type_image_webp);

    RET_IF("css", http_content_type_text_css);
    RET_IF("html", http_content_type_text_html);
    RET_IF("js", http_content_type_text_javascript);
    RET_IF("txt", http_content_type_text_plain);

    // unknown file extension
    return http_content_type_application_octet_stream;
}

#undef RET_IF
