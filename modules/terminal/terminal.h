#ifndef WB_TERMINAL_H
#define WB_TERMINAL_H

#include <stddef.h>
#include <sys/types.h>

#define WB_TERMINAL_LINE_BYTES 4096

typedef struct {
    pid_t child_pid;
    int master_fd;
    int running;
    int exited;
    int exit_status;
    char **lines;
    size_t line_count;
    size_t line_limit;
    size_t screen_floor;
    char current[WB_TERMINAL_LINE_BYTES];
    size_t current_len;
    size_t cursor;
    int parser_state;
    char csi[64];
    size_t csi_len;
} WbTerminalSession;

int wb_terminal_init(WbTerminalSession *s, size_t line_limit);
void wb_terminal_destroy(WbTerminalSession *s);
int wb_terminal_start(WbTerminalSession *s, const char *shell_override, int cols, int rows, char *err, size_t errn);
void wb_terminal_end(WbTerminalSession *s);
int wb_terminal_is_running(const WbTerminalSession *s);
int wb_terminal_master_fd(const WbTerminalSession *s);
int wb_terminal_resize(WbTerminalSession *s, int cols, int rows);
ssize_t wb_terminal_send(WbTerminalSession *s, const void *buf, size_t n);
int wb_terminal_pump(WbTerminalSession *s);
void wb_terminal_feed(WbTerminalSession *s, const unsigned char *buf, size_t n);
void wb_terminal_clear(WbTerminalSession *s);
void wb_terminal_clear_screen(WbTerminalSession *s);
size_t wb_terminal_screen_floor(const WbTerminalSession *s);
int wb_terminal_set_limit(WbTerminalSession *s, size_t line_limit);
size_t wb_terminal_line_count(const WbTerminalSession *s);
const char *wb_terminal_line_at(const WbTerminalSession *s, size_t index);
const char *wb_terminal_current_line(const WbTerminalSession *s);
int wb_terminal_exit_status(const WbTerminalSession *s);

#endif
