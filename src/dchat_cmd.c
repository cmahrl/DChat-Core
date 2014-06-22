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


/** @file dchat_cmd.c
 *  This file contains all available in-chat commands within this client.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <readline/readline.h>

#include "dchat_h/dchat_types.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"


/**
 * In-chat command to establish a new connection.
 * @param cnf Global config structure
 * @param cmd Buffer which contains the infos for the new connection
 * @return  -1 if error occurs, 0 if no error, -2 for invalid ip-address
 *          -3 for invalid port
 */
int
parse_cmd_connect(dchat_conf_t* cnf, char* cmd)
{
    struct sockaddr_storage ss;
    char* address;
    char* endptr;
    int r;
    char* tmpport;
    int port = 0;

    // if the string contains more spaces the pointer is after the loop at a non-space char
    for (; isspace(*cmd); cmd++);

    // is after the command no value the function returns
    if (*cmd == '\0')
    {
        return -1;
    }

    // the first of the command have to be the address
    address = strtok_r(cmd, " ", &endptr);

    // is there no port the functions returns -1 for error
    if ((tmpport = strtok_r(NULL, " \t\r\n", &endptr)) == NULL)
    {
        return -1;
    }

    port = atoi(tmpport);

    // check if the port is in a valid range
    if (port <= 0 || port > 65535)
    {
        return -3;
    }

    memset(&ss, 0, sizeof(ss));

    // check if the address is in the IPv4 format
    if (strchr(cmd, '.') != NULL)
    {
        // write the declared parameter to the address structure
        r = inet_pton(AF_INET, address, &((struct sockaddr_in*) &ss)->sin_addr);
        ((struct sockaddr*) &ss)->sa_family = AF_INET;
        ((struct sockaddr_in*) &ss)->sin_port = htons(port);
    }
    // check if address is in IPv6 format
    else if (strchr(cmd, ':') != NULL)
    {
        // write the declared parameter to the address structure
        r = inet_pton(AF_INET6, address, &((struct sockaddr_in6*) &ss)->sin6_addr);
        ((struct sockaddr*) &ss)->sa_family = AF_INET6;
        ((struct sockaddr_in6*) &ss)->sin6_port = htons(port);
    }
    else
    {
        // if address is not in IPv4 or IPv6 format an error is returned
        return -1;
    }

    if (r != 1)
    {
        // is the address invalid -2 is returned
        return -2;
    }

    // the new contact is written to the connector pipe
    if ((write(cnf->connect_fd[1], &ss, sizeof(ss)) == -1))
    {
        return -1;
    }

    return 0;
}


/**
 * In-chat command to show all available commands.
 * Function to establish new connections to other clients
 * @param cnf Global config structure
 */
void
parse_cmd_help(void)
{
    // print the available commands
    log_msg(LOG_INFO, "\nThe following commands are available: \n"
            "    %s"
            "    %s"
            "    %s"
            "    %s",
            "/connect <ip> <port>...connect to other chat client\n",
            "/exit..................close the chat program\n",
            "/help..................to show this helppage\n",
            "/list..................show all connected contacts\n");
}


/**
 *  In-chat command to list contacts of local contactlist.
 *  @param cnf Global config structure
 *  @return 0 if no error and -1 on error
 */
int
parse_cmd_list(dchat_conf_t* cnf)
{
    int i, count = 0;
    char address[INET6_ADDRSTRLEN + 1];
    unsigned short port;
    struct sockaddr_storage* client_addr;

    for (i = 0; i < cnf->cl.cl_size; i++)
    {
        // check if entry is a valid connection
        if (cnf->cl.contact[i].fd)
        {
            client_addr = &cnf->cl.contact[i].stor;

            // check the version of the ip address
            if (ip_version(client_addr) == 4)
            {
                // get the address from the sockaddr_storage structure
                inet_ntop(AF_INET, &(((struct sockaddr_in*) client_addr)->sin_addr), address,
                          INET_ADDRSTRLEN);
                // get the port from the structure
                port = ntohs(((struct sockaddr_in*) client_addr)->sin_port);
            }
            else if (ip_version(client_addr) == 6)
            {
                // get the address from the sockaddr_storage structure
                inet_ntop(AF_INET, &(((struct sockaddr_in6*) client_addr)->sin6_addr), address,
                          INET_ADDRSTRLEN);
                // get the port from the structure
                port = ntohs(((struct sockaddr_in6*) client_addr)->sin6_port);
            }
            else
            {
                return -1;
            }

            // print all available information about the connection
            dprintf(cnf->out_fd, "\n\n"
                    "    Contact................%d\n"
                    "    Address-Tupel..........%s:%u\n"
                    "    Listening-Port.........%u\n"
                    "    Chatroom...............%d", i, address, port, cnf->cl.contact[i].lport,
                    cnf->cl.contact[i].chatrooms);
            count++;
        }
    }

    // are there no contacts in the list a message will be printed
    if (!count)
    {
        log_msg(LOG_INFO, "No contacts found in the contactlist");
    }

    return 0;
}


/**
 *  Parses the given string and executes it if it is a command.
 *  @param cnf Global config structure
 *  @param buf Userinput
 *  @return 0 if the input was a command, -1 on error, -2 on syntax error
 */
int
parse_cmd(dchat_conf_t* cnf, char* buf)
{
    int ret;

    //check if the string contains the command /connect
    if (strncmp(buf, "/connect ", 9) == 0)
    {
        // to get the string without the command
        buf += 9;

        if ((ret = parse_cmd_connect(cnf, buf)) == -1)
        {
            log_msg(LOG_WARN, "Wrong Syntax! Syntax of connect is: /connect <IP> <PORT>");
            return -2;
        }
        else if (ret == -2)
        {
            log_msg(LOG_WARN, "Wrong IP address format");
            return -2;
        }
        else if (ret == -3)
        {
            log_msg(LOG_WARN, "Invalid Portnumber");
            return -2;
        }
    }
    // check if the command contains exit and return 1
    else if (strcmp(buf, "/exit") == 0)
    {
        raise(SIGTERM);
    }
    // check for the command help and call the appropriate function
    else if (strcmp(buf, "/help") == 0)
    {
        parse_cmd_help();
    }
    // check for the command list and call the appropriate function
    else if (strcmp(buf, "/list") == 0)
    {
        parse_cmd_list(cnf);
    }
    else
    {
        return -1;
    }

    return 0;
}
