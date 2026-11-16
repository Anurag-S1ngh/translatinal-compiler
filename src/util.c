#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

bool g_had_error = false;
const char *g_filename = "<unknown>";

StrBuf strbuf_new(void) {
    StrBuf buf;
    buf.capacity = 16;
    buf.length = 0;
    buf.data = xmalloc(buf.capacity);
    buf.data[0] = '\0';
    return buf;
}

void strbuf_append_char(StrBuf *buf, char c) {
    if (buf->length + 2 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = xrealloc(buf->data, buf->capacity);
    }
    buf->data[buf->length++] = c;
    buf->data[buf->length] = '\0';
}

void strbuf_append_str(StrBuf *buf, const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (buf->length + len + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = xrealloc(buf->data, buf->capacity);
    }
    memcpy(buf->data + buf->length, str, len);
    buf->length += len;
    buf->data[buf->length] = '\0';
}

void strbuf_append_fmt(StrBuf *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return;
    }

    while (buf->length + (size_t)needed + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = xrealloc(buf->data, buf->capacity);
    }

    vsnprintf(buf->data + buf->length, needed + 1, fmt, args_copy);
    buf->length += needed;
    va_end(args_copy);
}

void strbuf_free(StrBuf *buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->length = 0;
    buf->capacity = 0;
}

char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fatal_error("Could not open file '%s'", filename);
    }

    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = xmalloc(size + 1);
    size_t read_bytes = fread(buffer, 1, size, file);
    buffer[read_bytes] = '\0';

    fclose(file);
    return buffer;
}

char *read_stdin(void) {
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = xmalloc(capacity + 1);
    char chunk[4096];

    for (;;) {
        size_t n = fread(chunk, 1, sizeof(chunk), stdin);
        if (n == 0) {
            break;
        }
        while (length + n > capacity) {
            capacity *= 2;
        }
        buffer = xrealloc(buffer, capacity + 1);
        memcpy(buffer + length, chunk, n);
        length += n;
    }
    buffer[length] = '\0';
    return buffer;
}

void error_at(int line, int col, const char *fmt, ...) {
    g_had_error = true;
    fprintf(stderr, "%s:%d:%d: error: ", g_filename, line, col);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void fatal_error(const char *fmt, ...) {
    fprintf(stderr, "fatal error: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fatal_error("Out of memory (malloc %zu bytes)", size);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fatal_error("Out of memory (realloc %zu bytes)", size);
    }
    return new_ptr;
}

char *xstrdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = xmalloc(len);
    memcpy(copy, str, len);
    return copy;
}
