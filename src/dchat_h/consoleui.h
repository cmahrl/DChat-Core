#ifndef CONSOLEUI_H
#define CONSOLEUI_H

#include <syslog.h>
#include <stdarg.h>

#include "types.h"
#include "option.h"

#define LOG_WARN LOG_WARNING

int init_ui();
int ui_write(char* nickname, char* msg);
int ui_log(int lf,const char* fmt, ...);
void local_log(int lf, const char* fmt, ...);
int ui_log_errno(int lf, const char* fmt, ...);
void local_log_errno(int lf, const char* fmt, ...);
void ui_fatal(char* fmt, ...);
int vlog_msgf(int fd, int lf, const char* fmt, va_list ap, int with_errno);

void usage(int exit_status, cli_options_t* options, const char* fmt, ...);
void print_usage(int fd, cli_options_t* options);

typedef struct ipc
{
    char* path;
    int fd;
} ipc_t;


void ipc_connect();

void* th_ipc_reconnector(void* ptr);

void signal_reconnect();

int ui_read_line(char** line);

#endif