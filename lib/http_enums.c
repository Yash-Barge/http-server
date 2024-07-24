#define H_USE_INTERNAL_HTTP_ENUMS

#include "http_enums.h"

const char *http_status_codes_str[] = {
    [200] = "OK",
    [204] = "No Content",
    [301] = "Moved Permanently",
    [308] = "Permanent Redirect",
    [400] = "Bad Request",
    [404] = "Not Found",
    [405] = "Method Not Allowed"
};

const char *http_methods_str[] = { FOREACH_HTTP_METHOD(GENERATE_STRING) };
const char *http_headers_str[] = { FOREACH_HTTP_HEADER(HEADER_STRINGIFY) };
const char *http_content_type_str[] = { FOREACH_HTTP_CONTENT_TYPE(HTTP_CONTENT_TYPE_STRINGIFY) };
