#ifndef H_USE_INTERNAL_HTTP_ENUMS
    #error Do not include this header. Use the header file in the `include` directory.
#endif

#ifndef H_INTERNAL_HTTP_ENUMS
#define H_INTERNAL_HTTP_ENUMS

#include "sized_str.h"



extern const char *http_status_codes_str[];



#define GENERATE_ENUM(TOK) TOK,
#define GENERATE_STRING(str) #str,
#define FOREACH_HTTP_METHOD(macro) \
	macro(CONNECT) \
	macro(DELETE) \
	macro(GET) \
	macro(HEAD) \
	macro(OPTIONS) \
	macro(PATCH) \
	macro(POST) \
	macro(PUT) \
	macro(TRACE) \
	macro(METHOD_COUNT)

enum http_methods { FOREACH_HTTP_METHOD(GENERATE_ENUM) };
extern const char *http_methods_str[];
enum http_methods method_enumify(const struct sized_str str);



#define def_name1(pre, p1)                     pre ## p1
#define def_name2(pre, p1, p2)                 pre ## p1 ## _ ## p2
#define def_name3(pre, p1, p2, p3)             pre ## p1 ## _ ## p2 ## _ ## p3
#define def_name4(pre, p1, p2, p3, p4)         pre ## p1 ## _ ## p2 ## _ ## p3 ## _ ## p4
#define def_name5(pre, p1, p2, p3, p4, p5)     pre ## p1 ## _ ## p2 ## _ ## p3 ## _ ## p4 ## _ ## p5
#define def_name6(pre, p1, p2, p3, p4, p5, p6) pre ## p1 ## _ ## p2 ## _ ## p3 ## _ ## p4 ## _ ## p5 ## _ ## p6

#define def_name1_string(sep, sep2, p1)                     #p1
#define def_name2_string(sep, sep2, p1, p2)                 #p1 #sep #p2
#define def_name3_string(sep, sep2, p1, p2, p3)             #p1 #sep #p2 #sep2 #p3
#define def_name4_string(sep, sep2, p1, p2, p3, p4)         #p1 #sep #p2 #sep2 #p3 #sep2 #p4
#define def_name5_string(sep, sep2, p1, p2, p3, p4, p5)     #p1 #sep #p2 #sep2 #p3 #sep2 #p4 #sep2 #p5
#define def_name6_string(sep, sep2, p1, p2, p3, p4, p5, p6) #p1 #sep #p2 #sep2 #p3 #sep2 #p4 #sep2 #p5 #sep2 #p6

#define COUNT_N(_1, _2, _3, _4, _5, _6, N, ...) N
#define COUNT(...) COUNT_N(__VA_ARGS__ __VA_OPT__(,) 6, 5, 4, 3, 2, 1, 0) // WARN: Uses __VA_OPT__, not broad compiler support in C

#define DISPATCH(N) def_name##N
#define DISPATCH_STR(N) def_name##N##_string



#define APPLY(macro, ...) macro(__VA_ARGS__)
#define HEADER_ENUMIFY(...) APPLY(DISPATCH, COUNT(__VA_ARGS__))(http_header_, __VA_ARGS__),
#define HEADER_STRINGIFY(...) APPLY(DISPATCH_STR, COUNT(__VA_ARGS__))(-, -, __VA_ARGS__),

#define FOREACH_HTTP_HEADER(macro) \
    macro(accept, encoding) \
    macro(content, length) \
    macro(user, agent) \
	macro(count)

enum http_headers { FOREACH_HTTP_HEADER(HEADER_ENUMIFY) };
extern const char *http_headers_str[];



#define HTTP_CONTENT_TYPE_ENUMIFY(...) APPLY(DISPATCH, COUNT(__VA_ARGS__))(http_content_type_, __VA_ARGS__),
#define HTTP_CONTENT_TYPE_STRINGIFY(...) APPLY(DISPATCH_STR, COUNT(__VA_ARGS__))(/, -, __VA_ARGS__),

// ! application/octet-stream should remain first, so if content_type is omitted, defaults to this
#define FOREACH_HTTP_CONTENT_TYPE(macro) \
	macro(application, octet, stream) \
	\
	macro(image, avif) \
	macro(image, bmp) \
	macro(image, gif) \
	macro(image, jpeg) \
	macro(image, png) \
	macro(image, x, icon) \
	macro(image, webp) \
	\
	macro(text, css) \
	macro(text, html) \
	macro(text, javascript) \
	macro(text, plain)

enum http_content_type { FOREACH_HTTP_CONTENT_TYPE(HTTP_CONTENT_TYPE_ENUMIFY) };
extern const char *http_content_type_str[];
enum http_content_type get_file_type(const struct sized_str path);



#endif
