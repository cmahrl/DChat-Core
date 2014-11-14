#ifndef CONSOLEUI_H
#define CONSOLEUI_H

#include <syslog.h>
#include <stdarg.h>

#include "types.h"
#include "option.h"

#define LOG_WARN LOG_WARNING

int init_ui();
int ui_write(char* nickname, char* msg);
void ui_log(int lf,const char* fmt, ...);
void ui_log_errno(int lf, const char* fmt, ...);
void ui_fatal(char* fmt, ...);
void vlog_msgf(int fd, int lf, const char* fmt, va_list ap, int with_errno);

void usage(int exit_status, cli_options_t* options, const char* fmt, ...);
void print_usage(int fd, int exit_status, cli_options_t* options);
#endif