#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

#include "arena.h"
#include "http_enums.h"
#include "lib.h"
#include "sized_str.h"
#include "socket_queue.h"

#define DEFAULT_PORT 80
#define BUFFERSIZE 4096
#define THREAD_POOL_SIZE 20

// NOTE: serve directory defined in lib.c

__thread char *g_err_500_msg;

struct http_req {
    struct sized_str raw_req;
    enum http_methods method;
    struct sized_str url_path;
    struct sized_str user_agent;
    size_t content_length;
    size_t headers_length;
    int accept_compression;
    struct sized_str body;
};

struct http_reply {
    int status;
    enum http_content_type content_type;
    int content_encoding;
    struct sized_str location;
    struct sized_str body;
};

void log_req(const struct http_req *restrict req, const struct http_reply *restrict reply) {
    char *color;

    switch(reply->status / 100) {
        case 1:
            color = "\033[1;34m"; // blue
            break;
        case 2:
            color = "\033[1;32m"; // green
            break;
        case 3:
            color = "\033[1;36m"; // cyan
            break;
        case 4:
            color = "\033[1;35m"; // magenta
            break;
        case 5:
            color = "\033[1;31m"; // red
            break;
        default: // error
            color = "\033[1;33m"; // yellow
            break;
    }

    flockfile(stdout);
    flockfile(stderr);

    print_to_log("%s %.*s %s%d\033[0m",
        http_methods_str[req->method],
        (int) req->url_path.len, req->url_path.ptr,
        color, reply->status
    );

    if (reply->status / 100 == 5)
        fprintf(stderr, "\033[1;31merror:\033[0m %.*s\n", (int) reply->body.len, reply->body.ptr);

    funlockfile(stdout);
    funlockfile(stderr);

    return;
}

struct sized_str http_get_next_header(const struct sized_str *restrict req, const unsigned int offset) {
    struct sized_str retval = { .ptr = req->ptr + offset };

    for (unsigned int i = offset + 1; i < req->len; i++) {
        if (req->ptr[i] == '\n' && req->ptr[i-1] == '\r') {
            retval.len = i - offset - 1;
            break;
        }
    }

    if (!retval.len)
        retval.ptr = NULL;

    return retval;
}

void http_parse_header(const struct sized_str *restrict header_field, struct http_req *req) {
    int index = -1;
    int header_type;

    for (header_type = 0; header_type < http_header_count; header_type++)
        if ((index = post_prefix_index(*header_field, http_headers_str[header_type])) != -1)
            break;
    
    if (index == -1) // TODO: error condition ?
        return;
    
    while (header_field->ptr[index] == ':' || IS_WHITESPACE(header_field->ptr[index]))
        index++;

    switch (header_type) {
        int scanned_args;

        case http_header_accept_encoding:
            while (index < header_field->len) {
				if (IS_WHITESPACE(header_field->ptr[index]))
					index++;
				else if (strncmp(header_field->ptr + index, "gzip", 4)) {
					while (index < header_field->len && header_field->ptr[index] != ',')
						index++;
					if (header_field->ptr[index] == ',')
						index++;
				} else {
					req->accept_compression = 1;
					break;
				}
			}
            break;

        case http_header_content_length:
            scanned_args = sscanf(header_field->ptr + index, "%zu", &req->content_length);
            if (scanned_args != 1) // TODO: send back 400? what if body not necessary?
                ;
            break;

        case http_header_user_agent:
            req->user_agent = (struct sized_str) { .ptr = header_field->ptr + index, .len = header_field->len - index };
            break;

        default: // TODO: error condition (server-only headers) ?
            break;

    }

    return;
}

struct http_req *http_parse_req_headers(const char *raw_req, const size_t raw_req_len, struct arena *arena) {
    struct http_req *req = arena_alloc(arena, sizeof *req);
    *req = (struct http_req) { .raw_req = (struct sized_str) { .ptr = arena_alloc(arena, raw_req_len), .len = raw_req_len } };
    memcpy(req->raw_req.ptr, raw_req, raw_req_len);

    unsigned int offset = 0;

    {
        const struct sized_str req_line = http_get_next_header(&req->raw_req, offset);

        int method_end = -1, url_path_start = -1, url_path_end = -1;
		for (int i = 0; i < req_line.len; i++) {
			if (req_line.ptr[i] != ' ')
				continue;

			if (method_end == -1) {
				method_end = i;
                while (i+1 < req_line.len && req_line.ptr[i+1] == ' ')
                    i++;
                url_path_start = i+1;
			} else {
				url_path_end = i;
				break;
			}

		}

        if (method_end != -1 && url_path_end != -1) {
			req->method = method_enumify((struct sized_str) { .ptr = req_line.ptr, .len = method_end });
			req->url_path = (struct sized_str) { .ptr = req->raw_req.ptr+url_path_start, .len = url_path_end-url_path_start };
		}

		offset += req_line.len + 2;
    }

    while (offset < raw_req_len) {
        const struct sized_str header_field = http_get_next_header(&req->raw_req, offset);

        if (!header_field.len)
            break;

        http_parse_header(&header_field, req);

        offset += header_field.len + 2;
    }

    req->headers_length = offset + 2;

    return req;
}

struct http_reply *http_process_req(struct http_req *req, struct arena *arena) {
    struct http_reply *reply = arena_alloc(arena, sizeof *reply);
    int index;

    struct sized_str sanitized_url_path = validate_path(req->url_path, arena);

    if (!sanitized_url_path.len) // log_req has actual url_path
        goto bad_request;

    req->url_path = sanitized_url_path;

    if ((index = post_prefix_index(req->url_path, "/user-agent")) != -1) {
        if (req->method != GET && req->method != HEAD)
            goto method_not_allowed;

        if (req->url_path.len != index) {
            if (req->url_path.ptr[index] != '/' || req->url_path.len != index+1)
                goto not_found;

            *reply = (struct http_reply) { .status = 301, .location = (struct sized_str) { .ptr = "/user-agent", .len = 11 } };
            return reply;
        }

        *reply = (struct http_reply) {
            .status = 200,
            .body = req->user_agent,
            .content_type = http_content_type_text_plain
        };
    } else if ((index = post_prefix_index(req->url_path, "/echo")) != -1) {
        if (req->method != GET && req->method != HEAD)
            goto method_not_allowed;

        if (req->url_path.len == 5) {
            *reply = (struct http_reply) { .status = 301, .location = (struct sized_str) { .ptr = "/echo/", .len = 6 } };
            return reply;
        } else if (req->url_path.ptr[5] != '/')
            goto not_found;

        *reply = (struct http_reply) {
            .status = 200,
            .body = (struct sized_str) { .ptr = req->url_path.ptr+6, .len = req->url_path.len-6 },
            .content_type = http_content_type_text_plain
        };
    } else {
        // TODO: how to refactor this to be on a per route basis?
        if (req->method != GET && req->method != HEAD)
            goto method_not_allowed;

        const char d_or_f = dir_or_file(req->url_path, arena);
        switch (d_or_f) {
            char *temp_buf;
            struct sized_str file_content;

            case '\0':
                goto not_found;
            case 'e': // err_500 already set
                goto server_error;
            case 'd':
                if (req->url_path.ptr[req->url_path.len-1] != '/') { // enforce directory semantics
                    *reply = (struct http_reply) {
                        .status = 301,
                        .location = (struct sized_str) { .ptr = arena_alloc(arena, req->url_path.len+1), .len = req->url_path.len+1 }
                    };
                    memcpy(reply->location.ptr, req->url_path.ptr, req->url_path.len);
                    reply->location.ptr[req->url_path.len] = '/';

                    return reply;
                }

                temp_buf = arena_alloc(arena, req->url_path.len+10); // TODO: scratch?
                memcpy(temp_buf, req->url_path.ptr, req->url_path.len);
                memcpy(temp_buf+req->url_path.len, "index.html", 10);

                file_content = read_file((struct sized_str) { .ptr = temp_buf, .len = req->url_path.len+10 } , arena);
                if (g_err_500_msg)
                    goto server_error;
                else if (!file_content.len)
                    goto not_found;

                *reply = (struct http_reply) {
                    .status = 200,
                    .body = file_content,
                    .content_type = http_content_type_text_html
                };

                break;
            case 'f':
                if (req->url_path.len > 10 && !strncmp(req->url_path.ptr + req->url_path.len - 10, "index.html", 10)) {
                    *reply = (struct http_reply) {
                        .status = 301,
                        .location = (struct sized_str) { .ptr = arena_alloc(arena, req->url_path.len-10), .len = req->url_path.len-10 }
                    };
                    memcpy(reply->location.ptr, req->url_path.ptr, req->url_path.len-10);

                    return reply;
                }

                file_content = read_file(req->url_path, arena);
                if (!file_content.len) // file SHOULD exist, verified through dir_or_file (err_500 already set)
                    goto server_error;

                *reply = (struct http_reply) {
                    .status = 200,
                    .body = file_content,
                    .content_type = get_file_type(req->url_path)
                };

                break;
            default:
                fprintf(stderr, "Unchecked return type %c from dir_or_file()\n", d_or_f);
                set_err_500("Unchecked return type from dir_or_file()", arena);
                goto server_error;
        }
    }

    if (req->accept_compression && reply->body.len) { // TODO: add br compression? (no deflate)
        reply->content_encoding = 1;

        char *temp_buf = arena_alloc(arena, reply->body.len); // TODO: scratch arena?
        const int body_len = gzip_compress(temp_buf, reply->body);

        // if larger when compressed, send uncompressed
        // (when size is equal, most likely compression did not finish)
        if (body_len < reply->body.len)
            reply->body = (struct sized_str) { .ptr = temp_buf, .len = body_len };
        else
            reply->content_encoding = 0;
    }

    return reply;

bad_request:
    *reply = (struct http_reply) { .status = 400 };
    return reply;

not_found:
    *reply = (struct http_reply) { .status = 404 };

    struct sized_str file_content = read_file((struct sized_str) { .ptr = "/404.html", .len = 9 }, arena);
    if (g_err_500_msg)
        goto server_error;
    else if (file_content.len) {
        reply->body = file_content;
        reply->content_type = http_content_type_text_html;
    }

    return reply;

method_not_allowed:
    *reply = (struct http_reply) { .status = 405 };
    return reply;

server_error:
    *reply = (struct http_reply) {
        .status = 500,
        .body = (struct sized_str) { .ptr = g_err_500_msg, .len = strlen(g_err_500_msg) },
        .content_type = http_content_type_text_plain
    };

    g_err_500_msg = NULL;

    return reply;
}

// * could use `## __VA_ARGS__` (gcc/clang extension) instead of `__VA_OPT__(,) __VA_ARGS__`
#define APPEND_HEADER(fmt_string, ...) \
    do { \
        msg_len += snprintf(buffer+msg_len, BUFFERSIZE-msg_len, fmt_string __VA_OPT__(,) __VA_ARGS__); \
    } while (0)

struct sized_str http_prepare_res(struct http_reply *reply, struct http_req *req, struct arena *arena) {
    char *buffer = arena_alloc(arena, BUFFERSIZE); // TODO: scratch arena???
    size_t msg_len = 0;

    APPEND_HEADER("HTTP/1.1 %d %s\r\n", reply->status, http_status_codes_str[reply->status]);

    if (reply->status == 400)
        ; // skip the ladder
    else if (reply->status == 405)
        APPEND_HEADER("Allow: %s, %s\r\n", http_methods_str[GET], http_methods_str[HEAD]);
    else if (reply->status == 301)
        APPEND_HEADER("Location: %.*s\r\n", (int) reply->location.len, reply->location.ptr);
    else if (reply->body.len) {
        APPEND_HEADER("Content-Type: %s\r\n", http_content_type_str[reply->content_type]);

        if (reply->content_encoding)
            APPEND_HEADER("Content-Encoding: gzip\r\n");
    }

    if (reply->status >= 200 && reply->status != 204 && reply->status != 304)
        APPEND_HEADER("Content-Length: %zu\r\n", reply->body.len);

    APPEND_HEADER("\r\n");

    const size_t res_len = msg_len + (req->method == HEAD ? 0 : reply->body.len);

    char *res = arena_alloc(arena, res_len);
    memcpy(res, buffer, msg_len);

    if (req->method != HEAD)
        memcpy(res+msg_len, reply->body.ptr, reply->body.len);

    return (struct sized_str) { .ptr = res, .len = res_len };
}

#undef APPEND_HEADER

// TODO: transfer-encoding, and content-type: multipart
void *handle_client(void *args) {
    pthread_detach(pthread_self()); // TODO: this, or join after interrupt during cleanup?

    char buffer[BUFFERSIZE] = { 0 };
    size_t offset = 0;
    struct arena *arena = arena_new();

    while (1) {
        const int client_fd = dequeue();
        
        while (1) {
            int total_bytes_recvd = offset;
            const char *tracker;

            while (!(tracker = strstr(buffer, "\r\n\r\n")) && BUFFERSIZE-total_bytes_recvd) {
                int bytes_recvd;
                if ((bytes_recvd = recv(client_fd, buffer+total_bytes_recvd, BUFFERSIZE-total_bytes_recvd, 0)) < 0) { // TODO: add timeout
                    set_err_500("failed to read data from socket", arena);
                    goto processing_fasttrack;
                }

                total_bytes_recvd += bytes_recvd;
                if (!total_bytes_recvd && !bytes_recvd)
                    goto connection_terminated;
            }

            if ((BUFFERSIZE == total_bytes_recvd) && !tracker) {
                errno = EMSGSIZE;
                set_err_500("request too long, buffer length 4KiB", arena);
                goto processing_fasttrack;
            }
            
            struct http_req *req = http_parse_req_headers(buffer, total_bytes_recvd, arena);

            const size_t req_len = req->headers_length + req->content_length;
            if (req->content_length) {
                while (req_len > total_bytes_recvd) {
                    int bytes_recvd;
                    if ((bytes_recvd = recv(client_fd, buffer+total_bytes_recvd, BUFFERSIZE-total_bytes_recvd, 0)) < 0) { // TODO: add timeout
                        set_err_500("failed to read data from socket", arena);
                        goto processing_fasttrack;
                    }

                    total_bytes_recvd += bytes_recvd;
                }

                req->body = (struct sized_str) { .ptr = arena_alloc(arena, req->content_length), req->content_length };
                memcpy(req->body.ptr, buffer, req_len);
            }

            memmove(buffer, buffer+req_len, total_bytes_recvd-req_len);
            offset = total_bytes_recvd-req_len;
            memset(buffer+offset, 0, BUFFERSIZE-offset);

        processing_fasttrack:;
            struct http_reply *reply = http_process_req(req, arena);

            log_req(req, reply);

            const struct sized_str res = http_prepare_res(reply, req, arena);

            int total_bytes_sent = 0;

            while (total_bytes_sent < res.len) {
                const int bytes_sent = send(client_fd, res.ptr+total_bytes_sent, res.len-total_bytes_sent, 0);
                if (bytes_sent < 0) {
                    perror("\033[1;31merror:\033[0m send() failed, cannot respond to client");
                    goto connection_terminated;
                }
                total_bytes_sent += bytes_sent;
            }

            arena_clear(arena);
        }

    connection_terminated:
        if (shutdown(client_fd, SHUT_WR) < 0)
            perror("\033[1;31merror:\033[0m shutdown() of socket failed");

        close(client_fd);
    }

    arena_free(&arena);

    return NULL;
}

int main(void) {
    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
		pthread_create(&threads[i], NULL, handle_client, NULL);

    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    const int reuse = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) < 0)
		error_exit("setsockopt(SO_REUSEADDR)");

    {
        const struct sockaddr_in saddr = {
            .sin_family = AF_INET,
            .sin_port = htons(DEFAULT_PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY)
        };

        if (bind(socket_fd, (const struct sockaddr *) &saddr, sizeof saddr) < 0)
            error_exit("bind()");
    }

    const int connection_backlog = 0;
    if (listen(socket_fd, connection_backlog) < 0)
        error_exit("listen()");

    printf("Server online, awaiting connections...\n");

    struct sockaddr_in client_addr;
    int client_length = sizeof client_addr;

    while (1) {
        int client_fd;

        if ((client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_length)) < 0)
            perror("\033[1;31merror:\033[0m accept() failed, client dropped");

        enqueue(client_fd);
    }

    close(socket_fd);

    return 0;
}
