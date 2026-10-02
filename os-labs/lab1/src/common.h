#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

static inline int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static inline void write_str(int fd, const char *s) {
    (void)write_all(fd, s, strlen(s));
}

static inline void int_to_str(int value, char *out) {
    char tmp[12];
    size_t i = 0;
    long long v = value;
    int negative = v < 0;
    if (negative)
        v = -v;
    do {
        tmp[i++] = (char)('0' + v % 10);
        v /= 10;
    } while (v > 0);
    size_t j = 0;
    if (negative)
        out[j++] = '-';
    while (i > 0)
        out[j++] = tmp[--i];
    out[j] = '\0';
}

static inline void report_error(const char *prog, const char *what) {
    int saved = errno;
    write_str(STDERR_FILENO, prog);
    write_str(STDERR_FILENO, ": ");
    write_str(STDERR_FILENO, what);
    write_str(STDERR_FILENO, ": ");
    write_str(STDERR_FILENO, strerror(saved));
    write_str(STDERR_FILENO, "\n");
}

#endif
