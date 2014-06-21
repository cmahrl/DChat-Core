/*
 *  Copyright (c) 2014 Christoph Mahrl
 *
 *  This file is part of DChat.
 *
 *  DChat is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  DChat is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with DChat.  If not, see <http://www.gnu.org/licenses/>.
 */


/** @file util.c
 *  This file contains several miscellanious utility functions.
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


/**
 *  Determines the address family of the given socket address structure.
 *  @param address Pointer to address to check the address family for
 *  @return 4 if AF_INET is used, 6 if AF_INET6 is used or -1 in every other case
 */
int ip_version(struct sockaddr_storage* address)
{
    if (address->ss_family == AF_INET)
    {
        return 4;
    }
    else if (address->ss_family == AF_INET6)
    {
        return 6;
    }
    else
    {
        return -1;
    }
}


/**
 * Connects to a remote socket using the given socket address.
 * @param sa Pointer to initalized sockaddr structure.
 * @return file descriptor of new socket or -1 in case of error
 */
int connect_to(struct sockaddr* sa)
{
    int s; // socket file descriptor

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        log_errno(LOG_ERR, "socket() failed in connect_to()");
        return -1;
    }

    if (connect(s, sa, sizeof(struct sockaddr_in)) == -1)
    {
        log_errno(LOG_ERR, "connect() failed");
        close(s);
        return -1;
    }

    return s;
}


/**
 *  Prints the ansi escape code for clearing the current line in a
 *  terminal
 *  @param out_fd File descriptor where the escape code
 *                will be written to
 */
void ansi_term_clear_line(int out_fd)
{
    dprintf(out_fd, "\x1B[2K"); // erase stdin input
}


/**
 * Prints the ansi escape code for carriage return in a terminal
 * @param out_fd File descriptor where the escape code
 *               will be written to
 */
void ansi_term_cr(int out_fd)
{
    dprintf(out_fd, "\r");
}


/**
 * Returns the ansi escape code string for bold yellow color.
 */
char* ansi_color_bold_yellow()
{
    return "\x1B[1;33m";
}


/**
 * Returns the ansi escape code string for bold cyan color.
 */
char* ansi_color_bold_cyan()
{
    return "\x1B[1;36m";
}


/**
 * Returns the ansi escape code string for resetting attributes.
 */
char* ansi_reset_attributes()
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
 *  @param msg    Text message to print
 *  @param out_fd File descriptor where the message will be written to
 */
void print_dchat_msg(char* msg, int out_fd)
{
    // is file descriptor stdout or stderr?
    // if yes -> clear input from stdin in terminal and
    // return cursor to the beginning of the current line
    if (out_fd == 1 || out_fd == 2)
    {
        ansi_term_clear_line(out_fd);
        ansi_term_cr(out_fd);
    }
    
    dprintf(out_fd, "%s", ansi_color_bold_cyan()); // colorize msg
    dprintf(out_fd, "%s", msg); // print message
    dprintf(out_fd, "%s", ansi_reset_attributes()); // remote color

    // append \n if line was not terminated with \n
    if (msg[strlen(msg) - 1] != '\n')
    {
        dprintf(out_fd, "\n");
    }

    rl_forced_update_display(); // redraw stdin
}


/**
 *  Define the maximum of two given integers.
 *  @param a First integer
 *  @param b Second integer
 *  @return either a or b, depending on which one is bigger
 */
int max(int a, int b)
{
    return a > b ? a : b;
}
