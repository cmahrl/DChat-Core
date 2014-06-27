#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <syslog.h>

#define LOG_WARN LOG_WARNING


//*********************************
//       LOGGING FUNCTIONS
//*********************************
void vlog_msgf(FILE* out, int lf, const char* fmt, va_list ap);
void log_msg(int, const char*, ...) __attribute__((format(printf, 2, 3)));
void log_errno(int, const char*);
void log_hex(int, const void*, int);

//*********************************
//       PRINTING FUNCTIONS
//*********************************
void usage(const char* fmt, ...);
void fatal(const char* fmt, ...);
char* ansi_clear_line();
char* ansi_cr();
char* ansi_color_bold_yellow();
char* ansi_color_bold_cyan();
char* ansi_color_bold_red();
char* ansi_reset_attributes();
void print_dchat_msg(char* nickname, char* msg, int out_fd);



#endif
