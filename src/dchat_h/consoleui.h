#ifndef CONSOLEUI_H
#define CONSOLEUI_H

#include <syslog.h>

#include "types.h"
#include "option.h"

#define LOG_WARN LOG_WARNING

int init_ui(dchat_conf_t* cnf);
int ui_write(int fd, char* nickname, char* msg);
void ui_log(int fd, int lf,const char* fmt, ...);
void ui_log_errno(int fd, int lf, const char* fmt, ...);
void ui_fatal(int fd, char* fmt, ...);

void usage(int fd, int exit_status, cli_options_t* options, const char* fmt, ...);
void print_usage(int fd, int exit_status, cli_options_t* options);
#endif