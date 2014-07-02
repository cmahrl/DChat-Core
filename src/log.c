/** @file log.c
 *  This file contains functions to log messages with syslog loglevels to a file stream
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <readline/readline.h>

#include "dchat_h/log.h"
#include "dchat_h/option.h"


#ifndef LOG_PRI
#define LOG_PRI(p) ((p) & LOG_PRIMASK)
#endif

// FILE pointer to log
static FILE* log_ = NULL;
// log level
static int level_ = LOG_DEBUG;
static const char* flty_[8] = {"emerg", "alert", "crit", "err", "warning", "notice", "info", "debug"};


static void
__attribute__((constructor)) init_log0(void)
{
    log_ = stderr;
}


/**
 *  Log a message to a file.
 *  @param out Open FILE pointer
 *  @param lf Logging priority (equal to syslog)
 *  @param fmt Format string
 *  @param ap Variable parameter list
 */
void
vlog_msgf(FILE* out, int lf, const char* fmt, va_list ap)
{
    int level = LOG_PRI(lf);
    char buf[1024];

    if (level_ < level)
    {
        return;
    }

    if (out != NULL)
    {
        fprintf(out, "[%7s] ", flty_[level]);
        vfprintf(out, fmt, ap);
        fprintf(out, "\n");
    }
    else
    {
        vsnprintf(buf, sizeof(buf), fmt, ap);
        syslog(level | LOG_DAEMON, "%s", buf);
    }
}


/**
 *  Log a message. This function automatically determines to which streams the message is logged.
 *  @param lf Log priority
 *  @param fmt Format string
 *  @param ... arguments
 */
void
log_msg(int lf, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_msgf(log_, lf, fmt, ap);
    va_end(ap);
}


/**
 *  Log a message together with a string representation of errno as error.
 *  @param lf  Log priority
 *  @param str String to log
 */
void
log_errno(int lf, const char* str)
{
    log_msg(lf, "%s: '%s'", str, strerror(errno));
}


/**
 * This function output len bytes starting at buf in hexadecimal numbers.
 * @param lf  Log priority
 * @param buf Pointer to buffer
 * @param len Amount of bytes that shall be printed
 */
void
log_hex(int lf, const void* buf, int len)
{
    static const char hex[] = "0123456789abcdef";
    char tbuf[100];
    int i;

    for (i = 0; i < len; i++)
    {
        snprintf(tbuf + (i & 0xf) * 3, sizeof(tbuf) - (i & 0xf) * 3, "%c%c ",
                 hex[(((char*) buf)[i] >> 4) & 15], hex[((char*) buf)[i] & 15]);

        if ((i & 0xf) == 0xf)
        {
            log_msg(lf, "%s", tbuf);
        }
    }

    if ((i & 0xf))
    {
        log_msg(lf, "%s", tbuf);
    }
}


/**
 * Prints usage of this program.
 * @param exit_status Status of termination
 * @param options Array of options supported
 * @param size Size of array
 * @param Format string
 * @param ... arguments
 */
void
usage(int exit_status, cli_option_t* options, int size, const char* fmt, ...)
{
    if (strlen(fmt))
    {
        va_list args;
        va_start(args, fmt);
        vlog_msgf(log_, LOG_ERR, fmt, args);
        va_end(args);
    }

    fprintf(log_, "\n");
    fprintf(log_, " %s", PACKAGE_NAME);

    for (int i=0; i < size; i++)
    {
        if (options[i].mandatory_option)
        {
            if (options[i].mandatory_argument)
            {
                fprintf(log_, " -%c %s", options[i].opt, options[i].arg);
            }
            else
            {
                fprintf(log_, " -%c", options[i].opt);
            }
        }
        else
        {
            if (options[i].mandatory_argument)
            {
                fprintf(log_, " [-%c %s]", options[i].opt, options[i].arg);
            }
            else
            {
                fprintf(log_, " [-%c]", options[i].opt);
            }
        }
    }

    fprintf(log_, "\n\n");
    fprintf(log_, " Options:\n");

    for (int i=0; i < size; i++)
    {
        fprintf(log_, "%s\n\n", options[i].description);
    }

    fprintf(log_,
            " More detailed information can be found in the man page. See %s(1).\n",
            PACKAGE_NAME);
    exit(exit_status);
}


/**
 * Prints an error message and terminates this program.
 * @param fmt Format string
 * @param ... Arguments
 */
void
fatal(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog_msgf(log_, LOG_ERR, fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

/**
 *  Returns the ansi escape code string for clearing the current line
 *  in a terminal.
 */
char*
ansi_clear_line()
{
    return "\x1B[2K"; // erase stdin input
}


/**
 * Retruns the ansi escape code string for carriage return .
 */
char*
ansi_cr()
{
    return "\r";
}


/**
 * Returns the ansi escape code string for bold yellow color.
 */
char*
ansi_color_bold_yellow()
{
    return "\x1B[1;33m";
}


/**
 * Returns the ansi escape code string for bold cyan color.
 */
char*
ansi_color_bold_cyan()
{
    return "\x1B[1;36m";
}


/**
 * Returns the ansi escape code string for resetting attributes.
 */
char*
ansi_reset_attributes()
{
    return "\x1B[0m";
}


/**
 *  Prints a chat message to a file descriptor.
 *  Prints the given message string to the given output file descriptor.
 *  Before the message is printed, the current line in the terminal is
 *  cleared and the cursor is resetted to the beginning of the line.
 *  After the message is printed, the line buffer of GNU readline
 *  containing the userinput is reprinted to the terminal.
 *  @param nickanem Nickname of the client from whom we received a message
 *  @param msg      Text message to print
 *  @param out_fd File descriptor where the message will be written to
 */
void
print_dchat_msg(char* nickname, char* msg, int out_fd)
{
    dprintf(out_fd, "%s", ansi_clear_line());
    dprintf(out_fd, "%s", ansi_cr());
    dprintf(out_fd, "%s", ansi_color_bold_cyan());  // colorize msg
    dprintf(out_fd, "%s> ", nickname);              // print nickname
    dprintf(out_fd, "%s", ansi_reset_attributes()); // reset color
    dprintf(out_fd, "%s", msg);                     // print message

    // append \n if line was not terminated with \n
    if (msg[strlen(msg) - 1] != '\n')
    {
        dprintf(out_fd, "\n");
    }

    rl_forced_update_display(); // redraw stdin
}
