#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

#define PROG "child"
#define READ_CHUNK 4096

typedef struct {
    int fd;
    char buf[READ_CHUNK];
    size_t pos;
    size_t len;
} Reader;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Line;

static int line_push(Line *line, char c) {
    if (line->len == line->cap) {
        size_t new_cap = line->cap ? line->cap * 2 : 64;
        char *p = realloc(line->data, new_cap);
        if (p == NULL)
            return -1;
        line->data = p;
        line->cap = new_cap;
    }
    line->data[line->len++] = c;
    return 0;
}

static int read_line(Reader *r, Line *line) {
    line->len = 0;
    bool got_any = false;
    for (;;) {
        if (r->pos == r->len) {
            ssize_t n = read(r->fd, r->buf, sizeof r->buf);
            if (n == -1) {
                if (errno == EINTR)
                    continue;
                return -1;
            }
            if (n == 0)
                return got_any ? 1 : 0;
            r->pos = 0;
            r->len = (size_t)n;
        }
        char c = r->buf[r->pos++];
        got_any = true;
        if (c == '\n')
            return 1;
        if (line_push(line, c) == -1)
            return -1;
    }
}

static bool starts_with_capital(const char *s, size_t len) {
    return len > 0 && s[0] >= 'A' && s[0] <= 'Z';
}

int main(void) {
    Reader reader = {.fd = STDIN_FILENO, .pos = 0, .len = 0};
    Line line = {.data = NULL, .len = 0, .cap = 0};
    int exit_code = EXIT_SUCCESS;
    int rc;

    while ((rc = read_line(&reader, &line)) == 1) {
        if (line.len > 0 && line.data[line.len - 1] == '\r')
            line.len--;

        if (starts_with_capital(line.data, line.len)) {
            if (write_all(STDOUT_FILENO, line.data, line.len) == -1 ||
                write_all(STDOUT_FILENO, "\n", 1) == -1) {
                report_error(PROG, "failed to write to output file");
                exit_code = EXIT_FAILURE;
                break;
            }
        } else {
            static const char prefix[] = "Error: line \"";
            static const char suffix[] = "\" does not start with a capital letter\n";
            if (write_all(STDERR_FILENO, prefix, sizeof prefix - 1) == -1 ||
                write_all(STDERR_FILENO, line.data, line.len) == -1 ||
                write_all(STDERR_FILENO, suffix, sizeof suffix - 1) == -1) {
                exit_code = EXIT_FAILURE;
                break;
            }
        }
    }

    if (rc == -1) {
        report_error(PROG, "failed to read input");
        exit_code = EXIT_FAILURE;
    }

    free(line.data);
    return exit_code;
}
