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


/** @file cmdinterpreter.c
 *  This file contains all available in-chat commands within this client.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "dchat_h/cmdinterpreter.h"
#include "dchat_h/types.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"


/**
 *  Parses the given string and executes it if it is a command.
 *  @param cnf Global config structure
 *  @param line Userinput
 *  @return 0 if command was valid, 1 if command was invalid in all
 *  other cases -1 will be returned.
 */
int
parse_cmd(dchat_conf_t* cnf, char* line)
{
    cmds_t cmds;
    char* cmd;
    char* arg;
    char* delim = " ";
    int ret = -1;
    char* line_temp;

    if (init_cmds(&cmds) == -1)
    {
        return -1;
    }

    line_temp = malloc(strlen(line) + 1);

    if (line_temp == NULL)
    {
        fatal("Memory allocation for command line failed!");
    }

    line_temp[0] = '\0';
    strcat(line_temp, line);

    if ((cmd = strtok_r(line_temp, delim, &arg)) == NULL)
    {
        free(line_temp);
        return -1;
    }

    for (int i = 0; i < CMD_AMOUNT; i++)
    {
        if (!strcmp(cmd, cmds.cmd[i].cmd_name))
        {
            if ((ret = cmds.cmd[i].execute(cnf, arg)) == 1)
            {
                log_msg(LOG_NOTICE, "Command syntax: %s", cmds.cmd[i].syntax);
            }

            break;
        }
    }

    free(line_temp);
    return ret;
}


/**
 * Initializes a commands structure with all available in-chat commands and its
 * executable functions.
 * @param cmds Pointer to commands structure.
 * @return 0 on success, -1 otherwise.
 */
int
init_cmds(cmds_t* cmds)
{
    int temp_size;
    int cmds_size;
    // available commands
    cmd_t temp[] =
    {
        COMMAND(CMD_ID_HLP, CMD_NAME_HLP, CMD_ARG_HLP, hlp_exec),
        COMMAND(CMD_ID_CON, CMD_NAME_CON, CMD_ARG_CON, con_exec),
        COMMAND(CMD_ID_LST, CMD_NAME_LST, CMD_ARG_LST, lst_exec)
    };
    temp_size = sizeof(temp) / sizeof(temp[0]);

    if (temp_size > CMD_AMOUNT)
    {
        return -1;
    }

    memset(cmds, 0, sizeof(*cmds));
    memcpy(cmds->cmd, &temp, sizeof(temp));
    return 0;
}


/**
 * Prints help.
 * @return 0 on success, 1 on syntax error, -1 otherwise
 */
int
hlp_exec(dchat_conf_t* cnf, char* arg)
{
    cmds_t cmds;

    if (init_cmds(&cmds) == -1)
    {
        return -1;
    }

    log_msg(LOG_NOTICE, "Available Commands: ");

    for (int i = 0; i < CMD_AMOUNT; i++)
    {
        log_msg(LOG_NOTICE, "%s",cmds.cmd[i].syntax);
    }

    return 0;
}


/**
 * Connects to a remote host.
 * @return 0 on success, 1 on syntax error, -1 otherwise
 */
int
con_exec(dchat_conf_t* cnf, char* arg)
{
    char* address;
    char* port_str;
    int port;
    char* endptr;
    char* prefix;

    // if the string contains more spaces the pointer is after the loop at a non-space char
    if (remove_leading_spaces(arg) == NULL)
    {
        return 1;
    }

    address = strtok_r(arg, " ", &endptr);

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
 * Lists alls contacts within the local contactlist.
 * @return 0 on success, 1 on syntax error, -1 otherwise
 */
int
lst_exec(dchat_conf_t* cnf, char* arg)
{
    int i;

    // are there no contacts in the list a message will be printed
    if (!cnf->cl.used_contacts)
    {
        log_msg(LOG_NOTICE, "No contacts found in the contactlist");
    }
    else
    {
        for (i = 0; i < cnf->cl.cl_size; i++)
        {
            // check if entry is a valid connection
            if (cnf->cl.contact[i].fd)
            {
                log_msg(LOG_NOTICE, "");
                // print all available information about the connection
                log_msg(LOG_NOTICE, "Contact................%s", cnf->cl.contact[i].name);
                log_msg(LOG_NOTICE, "Onion-ID...............%s", cnf->cl.contact[i].onion_id);
                log_msg(LOG_NOTICE, "Hidden-Port............%hu", cnf->cl.contact[i].lport);
            }
        }
    }

    return 0;
}
