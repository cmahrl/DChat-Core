/** @file consoleui.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dchat_h/consoleui.h"

// log level
static int level_ = LOG_DEBUG;
static const char* flty_[8] = {"emerg", "alert", "crit", "err", "warning", "notice", "info", "debug"};

/**
 * Initializes input, output and log filedescriptor.
 * @return 0 on success, -1 in case of error
 */
int
init_ui()
{
    _cnf->in_fd            = 0;    // use stdin as input source
    _cnf->out_fd           = 1;    // use stdout as output target
    _cnf->log_fd           = 1;    // use stdout as log target
    return 0;
}

/**
 * Write recived message to out file descriptor.
 * @nickname Nickname of the client from whom we received the message
 * @msg Text message to print
 * @return 0 on success, -1 in case of error
*/
int
ui_write(char* nickname, char* msg)
{
    dprintf(_cnf->out_fd, "%s;%s\n", nickname, msg);
    return 0;
}

/**
*  Log a message to a filedescriptor.
*  @param fd File descritpor where the log will be written to
*  @param lf Logging priority (equal to syslog)
*  @param fmt Format string
*  @param ap Variable parameter list
*  @param with_errno Flag if errno should be printed too
*/
void
vlog_msgf(int fd, int lf, const char* fmt, va_list ap, int with_errno)
{
    int level = LOG_PRI(lf);
    char buf[1024];

    if (level_ < level)
    {
        return;
    }

    if (fd > -1)
    {
        dprintf(fd, "%s;", flty_[level]);
        vdprintf(fd, fmt, ap);

        if (with_errno)
        {
            dprintf(fd, " (%s)", strerror(errno));
        }

        dprintf(fd, "\n");
    }
    else
    {
        vsnprintf(buf, sizeof(buf), fmt, ap);
        syslog(level | LOG_DAEMON, "%s", buf);
    }
}


/**
 *  Log a message to log filedescriptor.
 *  @param lf Log priority
 *  @param fmt Format string
 *  @param ... arguments
 */
void
ui_log(int lf,const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_msgf(_cnf->log_fd, lf, fmt, ap, 0);
    va_end(ap);
}

/**
* Log a message to standard out
* @param lf Log priority
* @param fmt Format string
* @param ... arguments
*/
void
local_log(int lf, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_msgf(STDOUT_FILENO, lf, fmt, ap, 0);
    va_end(ap);
}

/**
 *  Log a message together with a string representation of errno as error to log filedescriptor.
 *  @param lf Log priority
 *  @param fmt Format string
 *  @param ... arguments
 */
void
ui_log_errno(int lf, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_msgf(_cnf->log_fd, lf, fmt, ap, 1);
    va_end(ap);
}

/**
 * Log a message together with a string representation of errno as error to standard out.
 *  @param lf Log priority
 *  @param fmt Format string
 *  @param ... arguments
 */
void
local_log_errno(int lf, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_msgf(STDOUT_FILENO, lf, fmt, ap, 1);
    va_end(ap);
}

/**
 * Prints an error message and terminates this program.
 * @param fmt Format string
 * @param ... Arguments
*/
void
ui_fatal(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog_msgf(_cnf->log_fd, LOG_ERR, fmt, args, 0);
    va_end(args);
    exit(EXIT_FAILURE);
}

/**
 * Prints usage of the program to stdout and log filedescriptor
 * @param exit_status Status of termination
 * @param options Array of options supported
 * @param Format string
 * @param ... arguments
 */
void
usage(int exit_status, cli_options_t* options, const char* fmt, ...)
{
    if (strlen(fmt))
    {
        va_list args;
        va_start(args, fmt);
        vlog_msgf(STDOUT_FILENO, LOG_ERR, fmt, args, 0);
        vlog_msgf(_cnf->log_fd, LOG_ERR, fmt, args, 0);
        va_end(args);
    }

    print_usage(STDOUT_FILENO, exit_status, options);
    print_usage(_cnf->log_fd, exit_status, options);
}

/**
 * Prints usage of this program.
 * @param fd File descriptor where the log will be written to
 * @param exit_status Status of termination
 * @param options Array of options supported
 */
void
print_usage(int fd, int exit_status, cli_options_t* options)
{
    dprintf(fd, "\n");
    dprintf(fd, " %s", PACKAGE_NAME);

    for (int i=0; i < CLI_OPT_AMOUNT; i++)
    {
        if (options->option[i].mandatory_option)
        {
            if (options->option[i].mandatory_argument)
            {
                dprintf(fd, " -%c %s", options->option[i].opt, options->option[i].arg);
            }
            else
            {
                dprintf(fd, " -%c", options->option[i].opt);
            }
        }
        else
        {
            if (options->option[i].mandatory_argument)
            {
                dprintf(fd, " [-%c %s]", options->option[i].opt, options->option[i].arg);
            }
            else
            {
                dprintf(fd, " [-%c]", options->option[i].opt);
            }
        }
    }

    dprintf(fd, "\n\n");
    dprintf(fd, " Options:\n");

    for (int i=0; i < CLI_OPT_AMOUNT; i++)
    {
        dprintf(fd, "%s\n\n", options->option[i].description);
    }

    dprintf(fd,
            " More detailed information can be found in the man page. See %s(1).\n",
            PACKAGE_NAME);
    exit(exit_status);
}