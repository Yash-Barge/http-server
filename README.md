
# HTTP server

Toy multithreaded HTTP/1.1 server, written in C without any external libraries (except `zlib`, used for HTTP compression).

## Pre-requisites

POSIX compliant OS only.

`zlib-dev` must be installed on system (tested with v1.2.11).

Tested with GCC v11.4.0 and Clang v15.0.0.

## Building and running

Execute `make` in the root directory.

## Features

### File-system based routing

Except for routes `/echo` and `/user-agent`, all routes are based on the folder structure (default serve directory is `serve/`, can be changed in `lib/lib.c`).

Each folder by default serves the `index.html` within that folder. Folders can be arbitrarily nested. Any CSS and JS files in the HTML document are also served. The most common image formats (except SVG) are also served.

### `gzip` compression

Serves compressed files based on request headers.

`deflate` omitted due to [cross-compatibility issues](https://stackoverflow.com/a/9186091).
