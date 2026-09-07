#define _XOPEN_SOURCE 700
#include "terminal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static char *dup_line(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void drop_oldest(WbTerminalSession *s, size_t n) {
    if (n > s->line_count) n = s->line_count;
    for (size_t i = 0; i < n; ++i) free(s->lines[i]);
    if (n < s->line_count) {
        memmove(s->lines, s->lines + n, (s->line_count - n) * sizeof(*s->lines));
    }
    s->line_count -= n;
    if (s->screen_floor >= n) s->screen_floor -= n;
    else s->screen_floor = 0;
}

static void push_line(WbTerminalSession *s) {
    if (!s->line_limit) {
        s->current_len = 0;
        s->cursor = 0;
        s->current[0] = '\0';
        return;
    }
    if (s->line_count >= s->line_limit) {
        drop_oldest(s, s->line_count - s->line_limit + 1);
    }
    char *p = dup_line(s->current);
    if (!p) return;
    s->lines[s->line_count++] = p;
    s->current_len = 0;
    s->cursor = 0;
    s->current[0] = '\0';
}

static size_t utf8_prev(const char *s, size_t pos) {
    if (!pos) return 0;
    size_t p = pos - 1;
    while (p > 0 && (((unsigned char)s[p] & 0xc0) == 0x80)) --p;
    return p;
}

static void put_byte(WbTerminalSession *s, unsigned char c) {
    if (s->cursor >= WB_TERMINAL_LINE_BYTES - 1) return;
    if (s->cursor < s->current_len) {
        s->current[s->cursor++] = (char)c;
    } else {
        s->current[s->current_len++] = (char)c;
        s->cursor = s->current_len;
        s->current[s->current_len] = '\0';
    }
}

static int csi_first(const WbTerminalSession *s, int def) {
    if (!s->csi_len) return def;
    char tmp[64];
    size_t n = s->csi_len < sizeof(tmp) - 1 ? s->csi_len : sizeof(tmp) - 1;
    memcpy(tmp, s->csi, n);
    tmp[n] = '\0';
    char *p = tmp;
    while (*p == '?' || *p == '>' || *p == '!') ++p;
    if (!*p) return def;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    return end == p ? def : (int)v;
}

static void apply_csi(WbTerminalSession *s, unsigned char final) {
    int p = csi_first(s, final == 'J' || final == 'K' ? 0 : 1);
    if (p < 0) p = 0;
    switch (final) {
        case 'J':
            if (p == 2 || p == 3) wb_terminal_clear_screen(s);
            break;
        case 'K':
            if (p == 2) {
                s->current_len = 0;
                s->cursor = 0;
                s->current[0] = '\0';
            } else if (p == 0) {
                if (s->cursor < s->current_len) s->current_len = s->cursor;
                s->current[s->current_len] = '\0';
            } else if (p == 1) {
                s->cursor = 0;
            }
            break;
        case 'G':
        case '`': {
            size_t pos = p > 0 ? (size_t)(p - 1) : 0;
            if (pos > s->current_len) pos = s->current_len;
            s->cursor = pos;
            break;
        }
        case 'C': {
            size_t add = (size_t)(p ? p : 1);
            while (add-- && s->cursor < WB_TERMINAL_LINE_BYTES - 1) {
                if (s->cursor >= s->current_len) put_byte(s, ' ');
                else ++s->cursor;
            }
            break;
        }
        case 'D': {
            size_t sub = (size_t)(p ? p : 1);
            while (sub-- && s->cursor) s->cursor = utf8_prev(s->current, s->cursor);
            break;
        }
        case 'P': {
            size_t del = (size_t)(p ? p : 1);
            if (s->cursor < s->current_len) {
                if (del > s->current_len - s->cursor) del = s->current_len - s->cursor;
                memmove(s->current + s->cursor,
                        s->current + s->cursor + del,
                        s->current_len - s->cursor - del + 1);
                s->current_len -= del;
            }
            break;
        }
        default:
            break;
    }
}

int wb_terminal_init(WbTerminalSession *s, size_t line_limit) {
    if (!s || !line_limit) return -1;
    memset(s, 0, sizeof(*s));
    s->master_fd = -1;
    s->child_pid = -1;
    s->line_limit = line_limit;
    s->lines = calloc(line_limit, sizeof(*s->lines));
    return s->lines ? 0 : -1;
}

void wb_terminal_clear(WbTerminalSession *s) {
    if (!s) return;
    drop_oldest(s, s->line_count);
    s->screen_floor = 0;
    s->current_len = 0;
    s->cursor = 0;
    s->current[0] = '\0';
    s->parser_state = 0;
    s->csi_len = 0;
}

void wb_terminal_clear_screen(WbTerminalSession *s) {
    if (!s) return;
    s->screen_floor = s->line_count;
    s->parser_state = 0;
    s->csi_len = 0;
}

size_t wb_terminal_screen_floor(const WbTerminalSession *s) {
    return s ? s->screen_floor : 0;
}

int wb_terminal_set_limit(WbTerminalSession *s, size_t line_limit) {
    if (!s || !line_limit) return -1;
    if (line_limit < s->line_count) drop_oldest(s, s->line_count - line_limit);
    char **p = realloc(s->lines, line_limit * sizeof(*p));
    if (!p) return -1;
    if (line_limit > s->line_limit) {
        memset(p + s->line_limit, 0, (line_limit - s->line_limit) * sizeof(*p));
    }
    s->lines = p;
    s->line_limit = line_limit;
    return 0;
}

void wb_terminal_feed(WbTerminalSession *s, const unsigned char *buf, size_t n) {
    if (!s || !buf) return;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = buf[i];
        if (s->parser_state == 3) {
            if (c == 7) s->parser_state = 0;
            else if (c == 27) s->parser_state = 4;
            continue;
        }
        if (s->parser_state == 4) {
            s->parser_state = c == '\\' ? 0 : 3;
            continue;
        }
        if (s->parser_state == 2) {
            if (c >= 0x40 && c <= 0x7e) {
                apply_csi(s, c);
                s->parser_state = 0;
                s->csi_len = 0;
                continue;
            }
            if (s->csi_len + 1 < sizeof(s->csi)) s->csi[s->csi_len++] = (char)c;
            continue;
        }
        if (s->parser_state == 1) {
            if (c == '[') {
                s->parser_state = 2;
                s->csi_len = 0;
            } else if (c == ']') {
                s->parser_state = 3;
            } else {
                s->parser_state = 0;
            }
            continue;
        }
        if (c == 27) {
            s->parser_state = 1;
            continue;
        }
        if (c == '\r') {
            s->cursor = 0;
            continue;
        }
        if (c == '\n') {
            push_line(s);
            continue;
        }
        if (c == '\b' || c == 127) {
            if (s->cursor) {
                size_t p = utf8_prev(s->current, s->cursor);
                if (s->cursor == s->current_len) {
                    s->current_len = p;
                    s->current[p] = '\0';
                }
                s->cursor = p;
            }
            continue;
        }
        if (c == '\t') {
            size_t spaces = 8 - (s->cursor % 8);
            while (spaces--) put_byte(s, ' ');
            continue;
        }
        if (c == '\f') {
            wb_terminal_clear(s);
            continue;
        }
        if (c < 0x20 || c == 0x7f) continue;
        put_byte(s, c);
    }
}

static const char *pick_shell(const char *override) {
    const char *s = override && *override ? override : getenv("SHELL");
    if (!s || !*s || access(s, X_OK) != 0) s = "/bin/sh";
    return s;
}

int wb_terminal_start(WbTerminalSession *s, const char *shell_override,
                      int cols, int rows, char *err, size_t errn) {
    if (!s) return -1;
    if (s->running) return 0;
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0) {
        if (err && errn) snprintf(err, errn, "posix_openpt: %s", strerror(errno));
        return -1;
    }
    if (grantpt(master) || unlockpt(master)) {
        if (err && errn) snprintf(err, errn, "grantpt/unlockpt: %s", strerror(errno));
        close(master);
        return -1;
    }
    char *slave_name = ptsname(master);
    if (!slave_name) {
        if (err && errn) snprintf(err, errn, "ptsname: %s", strerror(errno));
        close(master);
        return -1;
    }
    char slave_path[256];
    snprintf(slave_path, sizeof(slave_path), "%s", slave_name);
    const char *shell = pick_shell(shell_override);
    char shell_path[512];
    snprintf(shell_path, sizeof(shell_path), "%s", shell);
    pid_t pid = fork();
    if (pid < 0) {
        if (err && errn) snprintf(err, errn, "fork: %s", strerror(errno));
        close(master);
        return -1;
    }
    if (pid == 0) {
        if (setsid() < 0) _exit(126);
        int slave = open(slave_path, O_RDWR);
        if (slave < 0) _exit(126);
        if (ioctl(slave, TIOCSCTTY, 0) < 0) _exit(126);
        struct winsize ws = {
            (unsigned short)(rows > 0 ? rows : 24),
            (unsigned short)(cols > 0 ? cols : 80), 0, 0
        };
        ioctl(slave, TIOCSWINSZ, &ws);
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO) close(slave);
        close(master);
        if (!getenv("TERM")) setenv("TERM", "xterm-256color", 1);
        execl(shell_path, shell_path, "-i", (char *)NULL);
        execl("/bin/sh", "/bin/sh", "-i", (char *)NULL);
        _exit(127);
    }
    s->child_pid = pid;
    s->master_fd = master;
    s->running = 1;
    s->exited = 0;
    s->exit_status = 0;
    wb_terminal_resize(s, cols, rows);
    return 0;
}

static void reap_child(WbTerminalSession *s) {
    if (!s || s->child_pid <= 0) return;
    int st = 0;
    pid_t r = waitpid(s->child_pid, &st, WNOHANG);
    if (r == s->child_pid) {
        s->running = 0;
        s->exited = 1;
        s->exit_status = st;
        s->child_pid = -1;
        if (s->master_fd >= 0) {
            close(s->master_fd);
            s->master_fd = -1;
        }
    }
}

int wb_terminal_pump(WbTerminalSession *s) {
    if (!s) return -1;
    int total = 0;
    if (s->master_fd >= 0) {
        unsigned char buf[4096];
        for (;;) {
            ssize_t n = read(s->master_fd, buf, sizeof(buf));
            if (n > 0) {
                wb_terminal_feed(s, buf, (size_t)n);
                total += (int)n;
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (n < 0 && errno == EINTR) continue;
            if (n == 0 || (n < 0 && errno == EIO)) {
                reap_child(s);
                break;
            }
            break;
        }
    }
    reap_child(s);
    return total;
}

ssize_t wb_terminal_send(WbTerminalSession *s, const void *buf, size_t n) {
    if (!s || s->master_fd < 0 || !s->running) {
        errno = EPIPE;
        return -1;
    }
    return write(s->master_fd, buf, n);
}

int wb_terminal_resize(WbTerminalSession *s, int cols, int rows) {
    if (!s || s->master_fd < 0) return -1;
    struct winsize ws = {
        (unsigned short)(rows > 0 ? rows : 24),
        (unsigned short)(cols > 0 ? cols : 80), 0, 0
    };
    return ioctl(s->master_fd, TIOCSWINSZ, &ws);
}

int wb_terminal_is_running(const WbTerminalSession *s) { return s && s->running; }
int wb_terminal_master_fd(const WbTerminalSession *s) { return s ? s->master_fd : -1; }
int wb_terminal_exit_status(const WbTerminalSession *s) { return s ? s->exit_status : 0; }

void wb_terminal_end(WbTerminalSession *s) {
    if (!s || s->child_pid <= 0) {
        if (s) {
            s->running = 0;
            if (s->master_fd >= 0) {
                close(s->master_fd);
                s->master_fd = -1;
            }
        }
        return;
    }
    pid_t pid = s->child_pid;
    struct timespec ts = {0, 20000000};
    int st = 0;
    kill(-pid, SIGHUP);
    for (int i = 0; i < 15; ++i) {
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) goto done;
        nanosleep(&ts, NULL);
    }
    kill(-pid, SIGTERM);
    for (int i = 0; i < 10; ++i) {
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) goto done;
        nanosleep(&ts, NULL);
    }
    kill(-pid, SIGKILL);
    waitpid(pid, &st, 0);

done:
    s->exit_status = st;
    s->exited = 1;
    s->running = 0;
    s->child_pid = -1;
    if (s->master_fd >= 0) {
        close(s->master_fd);
        s->master_fd = -1;
    }
}

void wb_terminal_destroy(WbTerminalSession *s) {
    if (!s) return;
    wb_terminal_end(s);
    drop_oldest(s, s->line_count);
    free(s->lines);
    memset(s, 0, sizeof(*s));
    s->master_fd = -1;
    s->child_pid = -1;
}

size_t wb_terminal_line_count(const WbTerminalSession *s) { return s ? s->line_count : 0; }
const char *wb_terminal_line_at(const WbTerminalSession *s, size_t index) {
    return s && index < s->line_count ? s->lines[index] : NULL;
}
const char *wb_terminal_current_line(const WbTerminalSession *s) { return s ? s->current : ""; }
