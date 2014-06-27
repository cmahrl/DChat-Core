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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>

#include "dchat_h/types.h"
#include "dchat_h/log.h"


/**
 * In-chat command to establish a new connection.
 * @param cnf Global config structure
 * @param cmd Command buffer which contains the infos for the new connection
 * @return  -1 if error occurs, 0 if no error, 1 on syntax error
 */
int
parse_cmd_connect(dchat_conf_t* cnf, char* cmd)
{
    char* address;
    char* port_str;
    int port;
    char* endptr;
    char* prefix;

    // if the string contains more spaces the pointer is after the loop at a non-space char
    for (; isspace(*cmd); cmd++);

    // empty string?
    if (*cmd == '\0')
    {
        return 1;
    }

    address = strtok_r(cmd, " ", &endptr);

    if ((port_str = strtok_r(NULL, " \t\r\n", &endptr)) == NULL)
    {
        return 1;
    }

    port = (int) strtol(port_str, &endptr, 10);

    if (!is_valid_port(port) || *endptr != '\0')
    {
        log_msg(LOG_WARN, "Invalid port '%s'!", port_str);
        return 1;
    }

    if (!is_valid_onion(address))
    {
        log_msg(LOG_WARN, "Invalid onion-id '%s'!", address);
        return 1;
    }

    // write onion address to connector pipe
    if (write(cnf->connect_fd[1], address, ONION_ADDRLEN) == -1)
    {
        return -1;
    }

    if (write(cnf->connect_fd[1], &port, sizeof(uint16_t)) == -1)
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
            "/connect <onion-id> <port>...connect to other chat client\n",
            "/exit..................close the chat program\n",
            "/help..................to show this helppage\n",
            "/list..................show all connected contacts\n");
}


/**
 *  In-chat command to list contacts of local contactlist.
 *  @param cnf Global config structure
 */
void
parse_cmd_list(dchat_conf_t* cnf)
{
    int i;

    // are there no contacts in the list a message will be printed
    if (!cnf->cl.used_contacts)
    {
        log_msg(LOG_INFO, "No contacts found in the contactlist");
    }
    else
    {
        for (i = 0; i < cnf->cl.cl_size; i++)
        {
            // check if entry is a valid connection
            if (cnf->cl.contact[i].fd)
            {
                // print all available information about the connection
                dprintf(cnf->out_fd, "\n\n"
                        "    Contact................%s\n"
                        "    Onion-ID...............%s\n"
                        "    Listening-Port.........%u\n",
                        cnf->cl.contact[i].name,
                        cnf->cl.contact[i].onion_id,
                        cnf->cl.contact[i].lport);
            }
        }
    }
}


/**
 *  Parses the given string and executes it if it is a command.
 *  @param cnf Global config structure
 *  @param buf Userinput
 *  @return 0 if command was valid, 1 if command was invalid in all
 *  other cases -1 will be returned.
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
            return -1;
        }
        else if (ret == 1)
        {
            log_msg(LOG_ERR, "Syntax: /connect <ONION-ID> <PORT>");
            return 1;
        }
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
