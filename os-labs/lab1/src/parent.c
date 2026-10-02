#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#define PROG "parent"
#define CHILD_NAME "child"

static ssize_t read_first_line(char *buf, size_t cap) {
    size_t len = 0;
    for (;;) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0 || c == '\n')
            break;
        if (len + 1 >= cap) {
            errno = ENAMETOOLONG;
            return -1;
        }
        buf[len++] = c;
    }
    if (len > 0 && buf[len - 1] == '\r')
        len--;
    buf[len] = '\0';
    return (ssize_t)len;
}

static int build_child_path(char *out, size_t cap) {
    ssize_t len = readlink("/proc/self/exe", out, cap - 1);
    if (len == -1)
        return -1;
    if ((size_t)len >= cap - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    out[len] = '\0';

    char *slash = strrchr(out, '/');
    if (slash == NULL) {
        errno = ENOENT;
        return -1;
    }
    size_t dir_len = (size_t)(slash - out) + 1;
    size_t name_len = strlen(CHILD_NAME);
    if (dir_len + name_len + 1 > cap) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(out + dir_len, CHILD_NAME, name_len + 1);
    return 0;
}

static int relay_loop(int to_child, int from_child) {
    char pending[PIPE_BUF];
    size_t pending_len = 0;
    size_t pending_off = 0;
    char buf[4096];
    bool stdin_open = true;
    int result = 0;

    while (from_child != -1) {
        struct pollfd fds[3] = {
            {.fd = from_child, .events = POLLIN, .revents = 0},
            {.fd = (stdin_open && pending_len == 0) ? STDIN_FILENO : -1,
             .events = POLLIN, .revents = 0},
            {.fd = (pending_len > 0) ? to_child : -1,
             .events = POLLOUT, .revents = 0},
        };

        if (poll(fds, 3, -1) == -1) {
            if (errno == EINTR)
                continue;
            report_error(PROG, "poll failed");
            result = -1;
            break;
        }

        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n = read(from_child, buf, sizeof buf);
            if (n > 0) {
                if (write_all(STDOUT_FILENO, buf, (size_t)n) == -1) {
                    report_error(PROG, "failed to write to stdout");
                    result = -1;
                }
            } else if (n == 0) {
                close(from_child);
                from_child = -1;
            } else if (errno != EINTR) {
                report_error(PROG, "failed to read from pipe2");
                result = -1;
                close(from_child);
                from_child = -1;
            }
        }

        if (fds[1].fd != -1 && (fds[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t n = read(STDIN_FILENO, pending, sizeof pending);
            if (n > 0) {
                pending_len = (size_t)n;
                pending_off = 0;
            } else if (n == 0) {
                stdin_open = false;
            } else if (errno != EINTR) {
                report_error(PROG, "failed to read from stdin");
                result = -1;
                stdin_open = false;
            }
        }

        if (fds[2].fd != -1 && (fds[2].revents & (POLLOUT | POLLERR | POLLHUP))) {
            ssize_t n = write(to_child, pending + pending_off, pending_len - pending_off);
            if (n >= 0) {
                pending_off += (size_t)n;
                if (pending_off == pending_len)
                    pending_len = pending_off = 0;
            } else if (errno != EINTR && errno != EAGAIN) {
                report_error(PROG, "failed to write to pipe1");
                result = -1;
                pending_len = pending_off = 0;
                stdin_open = false;
                close(to_child);
                to_child = -1;
            }
        }

        if (!stdin_open && pending_len == 0 && to_child != -1) {
            close(to_child);
            to_child = -1;
        }
    }

    if (to_child != -1)
        close(to_child);
    return result;
}

int main(void) {
    char child_path[PATH_MAX];
    if (build_child_path(child_path, sizeof child_path) == -1) {
        report_error(PROG, "failed to locate child executable");
        return EXIT_FAILURE;
    }

    char filename[PATH_MAX];
    ssize_t name_len = read_first_line(filename, sizeof filename);
    if (name_len == -1) {
        report_error(PROG, "failed to read file name");
        return EXIT_FAILURE;
    }
    if (name_len == 0) {
        write_str(STDERR_FILENO, PROG ": file name is empty\n");
        return EXIT_FAILURE;
    }

    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        report_error(PROG, "failed to open output file");
        return EXIT_FAILURE;
    }

    int pipe1[2];
    int pipe2[2];
    if (pipe(pipe1) == -1) {
        report_error(PROG, "failed to create pipe1");
        close(file_fd);
        return EXIT_FAILURE;
    }
    if (pipe(pipe2) == -1) {
        report_error(PROG, "failed to create pipe2");
        close(file_fd);
        close(pipe1[0]);
        close(pipe1[1]);
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        report_error(PROG, "failed to ignore SIGPIPE");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        report_error(PROG, "failed to fork");
        close(file_fd);
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        signal(SIGPIPE, SIG_DFL);

        if (dup2(pipe1[0], STDIN_FILENO) == -1 ||
            dup2(file_fd, STDOUT_FILENO) == -1 ||
            dup2(pipe2[1], STDERR_FILENO) == -1) {
            report_error(PROG " (child branch)", "dup2 failed");
            _exit(EXIT_FAILURE);
        }

        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);
        close(file_fd);

        char *const args[] = {(char *)CHILD_NAME, NULL};
        execv(child_path, args);

        report_error(CHILD_NAME, "failed to exec");
        _exit(127);
    }

    close(pipe1[0]);
    close(pipe2[1]);
    close(file_fd);

    int relay_result = relay_loop(pipe1[1], pipe2[0]);

    int wstatus;
    while (waitpid(pid, &wstatus, 0) == -1) {
        if (errno != EINTR) {
            report_error(PROG, "waitpid failed");
            return EXIT_FAILURE;
        }
    }

    bool child_ok = false;
    char num[12];
    if (WIFEXITED(wstatus)) {
        int code = WEXITSTATUS(wstatus);
        child_ok = (code == 0);
        if (!child_ok) {
            int_to_str(code, num);
            write_str(STDERR_FILENO, PROG ": child exited with code ");
            write_str(STDERR_FILENO, num);
            write_str(STDERR_FILENO, "\n");
        }
    } else if (WIFSIGNALED(wstatus)) {
        int_to_str(WTERMSIG(wstatus), num);
        write_str(STDERR_FILENO, PROG ": child killed by signal ");
        write_str(STDERR_FILENO, num);
        write_str(STDERR_FILENO, "\n");
    }

    return (relay_result == 0 && child_ok) ? EXIT_SUCCESS : EXIT_FAILURE;
}
