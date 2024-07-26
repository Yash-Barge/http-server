
# HTTP server

Toy multithreaded HTTP/1.1 server, written in C without any external libraries (except `zlib`, used for HTTP compression).

## Pre-requisites

POSIX compliant OS only.

`zlib-dev` must be installed on system (tested with v1.2.11).

Tested with GCC v11.4.0 and Clang v15.0.0.

## Building and running

Execute `make` in the root directory. Server listens on port 80, so open `localhost` on browser (or `curl localhost`).

## Features

### File-system based routing

Except for routes `/echo` and `/user-agent`, all routes are based on the folder structure (default serve directory is `serve/`, can be changed in `lib/lib.c`).

Each folder by default serves the `index.html` within that folder. Folders can be arbitrarily nested. Any CSS and JS files in the HTML document are also served. The most common image formats (except SVG) are also served.

Server correctly responds with status code 404 is the requested file is not available, validates the path (in case of `curl --path-as-is localhost/..`, it will respond with status code 400) and responds with error code 500 should any unexpected error occur.

### `gzip` compression

Serves compressed files based on request headers.

`deflate` omitted due to [cross-compatibility issues](https://stackoverflow.com/a/9186091).

## Technical implementation details

### Threading

1. Utilizes POSIX threading model.
2. Uses mutexes and condition variables to synchronize thread access (in `lib/socket_queue.c`).

### Arena allocators

Arena allocators are used extensively throughout the codebase, replacing almost all usage of `malloc` and `free`.
