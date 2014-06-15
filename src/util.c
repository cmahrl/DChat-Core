/*!
 * =====================================================================================
 *
 *       Filename:  util.c
 *
 *    Description:  This file contains several miscellanious utility functions.
 *
 *        Version:  1.0
 *        Created:  06/08/14 09:34:39
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Christoph Mahrl (clm), christoph.mahrl@gmail.com
 *   Organization:  IT-Security
 *
 * =====================================================================================
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <readline/readline.h>

#include "dchat_h/log.h"


/*! Determine the address family of the given address.
 *  @param address: Pointer to address to check the address family for
 *  @return 4 if AF_INET is used, 6 if AF_INET6 is used or -1 in every other case
 */
int ip_version(struct sockaddr_storage* address)
{
    if(address->ss_family == AF_INET)
    {
        return 4;
    }

    else if(address->ss_family == AF_INET6)
    {
        return 6;
    }

    else
    {
        return -1;
    }
}


/*!Function connects to remote socket described by sa.
 * @param sa pointer to initalized sockaddr structure.
 * @return Returns file descriptor of new socket or -1 in case of error
 */
int connect_to(struct sockaddr* sa)
{
    int s; //!< socket file descriptor

    if((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        log_errno(LOG_ERR, "socket() failed in connect_to()");
        return -1;
    }

    if(connect(s, sa, sizeof(struct sockaddr_in)) == -1)
    {
        log_errno(LOG_ERR, "connect() failed");
        close(s);
        return -1;
    }

    return s;
}


/*! Prints the ansi escape code for clearing the current line
 *  in a terminal
 *  @param out_fd: File descriptor where the escape code
 *                will be written to
 */
void ansi_term_clear_line(int out_fd)
{
    dprintf(out_fd, "\x1B[2K"); //!< erase stdin input
}


/*! Prints the ansi escape code for carriage return in a terminal
 * @param out_fd: File descriptor where the escape code
 *                will be written to
 */
void ansi_term_cr(int out_fd)
{
    dprintf(out_fd, "\r");
}


/*! Prints the given message string to the given output file descriptor.
 *  @param msg:    Text message to print
 *  @param out_fd: File descriptor where the message will be written to
 */
void print_dchat_msg(char* msg, int out_fd)
{
    //!< is file descriptor stdout or stderr?
    //!< if yes -> clear input from stdin in terminal and
    //!< return cursor to the beginning of the current line
    if(out_fd == 1 || out_fd == 2)
    {
        ansi_term_clear_line(out_fd);
        ansi_term_cr(out_fd);
    }

    dprintf(out_fd, "%s", msg); //!< print message

    //!< append \n if line was not terminated with \n
    if(msg[strlen(msg) - 1] != '\n')
    {
        dprintf(out_fd, "\n");
    }

    rl_forced_update_display(); //!< redraw stdin
}


/*! Define the maximum of two given integers.
 *  @param a: first integer
 *  @param b: second integer
 *  @return either a or b, depending on which one is bigger
 */
int max(int a, int b)
{
    return a > b ? a : b;
}
