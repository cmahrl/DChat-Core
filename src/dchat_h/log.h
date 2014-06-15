#ifndef LOG_H
#define LOG_H

#include <syslog.h>

#define LOG_WARN LOG_WARNING

void log_msg(int, const char*, ...) __attribute__((format(printf, 2, 3)));
void log_errno(int, const char*);
void log_hex(int, const void*, int);


#endif
