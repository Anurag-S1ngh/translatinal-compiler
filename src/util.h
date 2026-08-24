#ifndef TRCO_UTIL_H
#define TRCO_UTIL_H

#include <stddef.h>
#include <stdbool.h>

// Dynamic String Buffer
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StrBuf;

StrBuf strbuf_new(void);
void strbuf_append_char(StrBuf *buf, char c);
void strbuf_append_str(StrBuf *buf, const char *str);
void strbuf_append_fmt(StrBuf *buf, const char *fmt, ...);
void strbuf_free(StrBuf *buf);

// File I/O
char *read_file(const char *filename);

// Error Handling & Diagnostics
extern bool g_had_error;
extern const char *g_filename;

void error_at(int line, int col, const char *fmt, ...);
void fatal_error(const char *fmt, ...);

// Safe memory allocation helpers
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);

#endif // TRCO_UTIL_H
