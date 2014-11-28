/** @file consoleui.c
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dchat_h/consoleui.h"
#include "dchat_h/decoder.h"

// log level
static int level_ = LOG_DEBUG;
static const char* flty_[8] = {"emerg", "alert", "crit", "err", "warning", "notice", "info", "debug"};

static int _recival = 5;
static int _reconnect = 0;
static pthread_mutex_t _lock;
static pthread_cond_t _cond;
static pthread_mutex_t _lock2;
static pthread_cond_t _cond2;

static ipc_t _ipc_inp;
static ipc_t _ipc_out;
static ipc_t _ipc_log;

static pthread_t _th_acpt_inp;
static pthread_t _th_acpt_out;
static pthread_t _th_acpt_log;

static pthread_t _th_rec;

/**
 * Initializes input, output and log filedescriptor.
 * @return 0 on success, -1 in case of error
 */
int
init_ui()
{
    _ipc_inp.path = INP_SOCK_PATH;
    _ipc_out.path = OUT_SOCK_PATH;
    _ipc_log.path = LOG_SOCK_PATH;

    // mutex for reconnect
    if (pthread_mutex_init(&_lock, NULL) != 0)
    {
        return -1;
    }

    // reconnect condition
    if (pthread_cond_init(&_cond, NULL) != 0)
    {
        return -1;
    }

    pthread_mutex_init(&_lock2, NULL);
    pthread_cond_init(&_cond2, NULL);
    signal(SIGPIPE, SIG_IGN);
    ipc_connect();
    pthread_create(&_th_rec, NULL, (void*) th_ipc_reconnector, NULL);
    return 0;
}

/**
 * Closes all open sockets
 */
void
free_unix_socks()
{
    if (_cnf->in_fd > 2)
    {
        close(_cnf->in_fd);
        _cnf->in_fd = -1;
    }

    if (_cnf->out_fd > 2)
    {
        close(_cnf->out_fd);
        _cnf->out_fd = -1;
    }

    if (_cnf->log_fd > 2)
    {
        close(_cnf->log_fd);
        _cnf->log_fd = -1;
    }
}

void
unix_accept_error(char* error)
{
    local_log_errno(1, error);
    // cancel all accept threads
    pthread_cancel(_th_acpt_inp);
    pthread_cancel(_th_acpt_out);
    pthread_cancel(_th_acpt_log);
}

void
unix_accept(ipc_t* ipc)
{
    int fd;
    struct sockaddr_un sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));

    // ENOENT means file does not exist
    if (unlink(ipc->path) < 0 && errno != ENOENT)
    {
        unix_accept_error("Unlink path failed!");
        pthread_testcancel();
    }

    if ((ipc->fd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1)
    {
        unix_accept_error("Creation of socket failed!");
        pthread_testcancel();
    }

    sock_addr.sun_family = PF_LOCAL;
    strcpy(sock_addr.sun_path, ipc->path);

    if (bind(ipc->fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) == -1)
    {
        unix_accept_error("Binding to socket failed!");
        pthread_testcancel();
    }

    if (listen(ipc->fd, 1) == -1)
    {
        unix_accept_error("Listening failed!");
        pthread_testcancel();
    }

    local_log(1, "waiting for connect %s", ipc->path);

    if ((fd = accept(ipc->fd, 0, 0)) == -1)
    {
        unix_accept_error("Accepting new connection failed!");
        pthread_testcancel();
    }

    // close accept fd
    close(ipc->fd);
    // set ptr->fd from accept fd to actual filedescriptor
    ipc->fd = fd;
    local_log(1, "Connected to %s", ipc->path);
}

void
ipc_accept_cleanup(void* ptr)
{
    ipc_t* ipc = (ipc_t*)ptr;
    // close accept fd
    close(ipc->fd);
    ipc->fd = -1;
    // unlink path
    unlink(ipc->path);
}

void*
th_ipc_accept(void* ptr)
{
    ipc_t* ipc = (ipc_t*)ptr;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_cleanup_push(&ipc_accept_cleanup, &ipc->path);
    unix_accept(ipc);

    // filedescriptor mus be higher than 2;
    if (ipc->fd <= 2)
    {
        ipc->fd = -1;
    }

    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

void
ipc_connect()
{
    while (1)
    {
        free_unix_socks();
        pthread_create(&_th_acpt_inp, NULL, (void*) th_ipc_accept, &_ipc_inp);
        pthread_create(&_th_acpt_out, NULL, (void*) th_ipc_accept, &_ipc_out);
        pthread_create(&_th_acpt_log, NULL, (void*) th_ipc_accept, &_ipc_log);
        pthread_join(_th_acpt_inp, NULL);
        pthread_join(_th_acpt_out, NULL);
        pthread_join(_th_acpt_log, NULL);
        _cnf->in_fd = _ipc_out.fd;
        _cnf->out_fd = _ipc_inp.fd;
        _cnf->log_fd = _ipc_log.fd;

        if (_cnf->in_fd != -1 && _cnf->out_fd != -1 && _cnf->log_fd != -1)
        {
            break;
        }

        sleep(_recival);
        local_log(1, "");
        local_log(1, "RETRY");
        local_log(1, "");
    }

    ui_write(_cnf->me.name, "");

    local_log(1, "in_fd: %d", _cnf->in_fd);
    local_log(1, "out_fd: %d", _cnf->out_fd);
    local_log(1, "log_fd: %d", _cnf->log_fd);
    local_log(1, "");
}

void
signal_reconnect(char* src)
{
    local_log(1,"ENTER RECONNECT %s", src);
    pthread_mutex_lock(&_lock);
    local_log_errno(1, "cond_signal ret: %d", pthread_cond_signal(&_cond));
    pthread_mutex_unlock(&_lock);
    pthread_mutex_lock(&_lock2);

    if (_reconnect)
    {
        pthread_cond_wait(&_cond2, &_lock2);
    }

    pthread_mutex_unlock(&_lock2);
    local_log(1,"EXIT RECONNECT %s", src);
}

void*
th_ipc_reconnector(void* ptr)
{
    while (1)
    {
        pthread_mutex_lock(&_lock);
        pthread_cond_wait(&_cond, &_lock);
        _reconnect = 1;
        pthread_mutex_unlock(&_lock);
        local_log(1, "\nRECONNECTING...\n");
        ipc_connect();
        pthread_mutex_lock(&_lock2);
        _reconnect = 0;
        pthread_cond_broadcast(&_cond2);
        pthread_mutex_unlock(&_lock2);
    }

    free_unix_socks();
    pthread_mutex_destroy(&_lock);
    pthread_cond_destroy(&_cond);
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
    if (dprintf(_cnf->out_fd, "%s;%s\n", nickname, msg) < 0)
    {
        signal_reconnect("out");
        return -1;
    }

    return 0;
}

/**
*  Log a message to a filedescriptor.
*  @param fd File descritpor where the log will be written to
*  @param lf Logging priority (equal to syslog)
*  @param fmt Format string
*  @param ap Variable parameter list
*  @param with_errno Flag if errno should be printed too
*  @return 0 on success, -1 in case of error
*/
int
vlog_msgf(int fd, int lf, const char* fmt, va_list ap, int with_errno)
{
    int level = LOG_PRI(lf);
    char buf[1024];

    if (level_ < level)
    {
        return 0;
    }

    if (fd > -1)
    {
        if (dprintf(fd, "%s;", flty_[level]) < 0)
        {
            signal_reconnect("vlog");
            return -1;
        }

        if (vdprintf(fd, fmt, ap) < 0)
        {
            signal_reconnect("vlog");
            return -1;
        }

        if (with_errno)
        {
            if (dprintf(fd, " (%s)", strerror(errno)) < 0)
            {
                signal_reconnect("vlog");
                return -1;
            }
        }

        if (dprintf(fd, "\n") < 0)
        {
            signal_reconnect("vlog");
            return -1;
        }
    }
    else
    {
        if (vsnprintf(buf, sizeof(buf), fmt, ap) < 0)
        {
            signal_reconnect("vlog");
            return -1;
        }

        syslog(level | LOG_DAEMON, "%s", buf);
    }

    return 0;
}


/**
 *  Log a message to log filedescriptor.
 *  @param lf Log priority
 *  @param fmt Format string
 *  @param ... arguments
 *  @return 0 on sucess, -1 in case of error
 */
int
ui_log(int lf,const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if ((vlog_msgf(_cnf->log_fd, lf, fmt, ap, 0)) < 0)
    {
        return -1;
    }

    va_end(ap);
    return 0;
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
 *  @return 0 on sucess, -1 in case of error
 */
int
ui_log_errno(int lf, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if ((vlog_msgf(_cnf->log_fd, lf, fmt, ap, 1)) < 0)
    {
        return -1;
    }

    va_end(ap);
    return 0;
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
 * Prints an error message to stdout and log filedescriptor and terminates this program.
 * @param fmt Format string
 * @param ... Arguments
*/
void
ui_fatal(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog_msgf(STDOUT_FILENO, LOG_ERR, fmt, args, 0);

    // only write to log_fd if it is already initialized
    if (_cnf->log_fd)
    {
        vlog_msgf(_cnf->log_fd, LOG_ERR, fmt, args, 0);
    }

    va_end(args);
    exit(EXIT_FAILURE);
}

/**
 * Prints usage of the program to stdout and log filedescriptor and terminates this program.
 * @param exit_status Status of termination
 * @param options Array of options supported
 * @param Format string
 * @param ... arguments
 * @return 0 on success, -1 in case of error
 */
void
usage(int exit_status, cli_options_t* options, const char* fmt, ...)
{
    if (strlen(fmt))
    {
        va_list args;
        va_start(args, fmt);
        vlog_msgf(STDOUT_FILENO, LOG_ERR, fmt, args, 0);

        // only write to log_fd if it is already initialized
        if (_cnf->log_fd)
        {
            vlog_msgf(_cnf->log_fd, LOG_ERR, fmt, args, 0);
        }

        va_end(args);
    }

    print_usage(STDOUT_FILENO, options);

    // only write to log_fd if it is already initialized
    if (_cnf->log_fd)
    {
        print_usage(_cnf->log_fd, options);
    }

    exit(exit_status);
}

/**
 * Prints usage of this program.
 * @param fd File descriptor where the log will be written to
 * @param exit_status Status of termination
 * @param options Array of options supported
 */
void
print_usage(int fd, cli_options_t* options)
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
}

int
ui_read_line(char** line)
{
    int ret = read_line(_cnf->in_fd, line);

    if (ret <= 0)
    {
        signal_reconnect("in");
    }

    return ret;
}