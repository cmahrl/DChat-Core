/*
 * =====================================================================================
 *
 *       Filename:  dchat_cmd.c
 *
 *    Description:  This file contains all available commands which can be used
 *                  in the dchat client
 *        Version:  1.0
 *        Created:  05/06/2014
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Authors: Christoph Mahrl (clm), christoph.mahrl@gmail.com
 *   Organization:  University of Applied Sciences St. Poelten - IT-Security
 *
 * =====================================================================================
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


/*!Function to establish new connections to other clients
 * @param cnf the global config structure
 * @param cmd the buffer which contains the infos for the new connection
 * @return  -1 if error occurs, 0 if no error, -2 for invalid ip-address
 *          -3 for invalid port
 */
int parse_cmd_connect(dchat_conf_t* cnf, char* cmd)
{
    struct sockaddr_storage ss;
    char* address;
    char* endptr;
    int r;
    char* tmpport;
    int port = 0;

    //!< if the string contains more spaces the pointer is after the loop at a non-space char
    for(; isspace(*cmd); cmd++);

    //!< is after the command no value the function returns
    if(*cmd == '\0')
    {
        return -1;
    }

    //!< the first of the command have to be the address
    address = strtok_r(cmd, " ", &endptr);

    //!< is there no port the functions returns -1 for error
    if((tmpport = strtok_r(NULL, " \t\r\n", &endptr)) == NULL)
    {
        return -1;
    }

    port = atoi(tmpport);

    //!< check if the port is in a valid range
    if(port <= 0 || port > 65535)
    {
        return -3;
    }

    memset(&ss, 0, sizeof(ss));

    //!< check if the address is in the IPv4 format
    if(strchr(cmd, '.') != NULL)
    {
        //!< write the declared parameter to the address structure
        r = inet_pton(AF_INET, address, &((struct sockaddr_in*) &ss)->sin_addr);
        ((struct sockaddr*) &ss)->sa_family = AF_INET;
        ((struct sockaddr_in*) &ss)->sin_port = htons(port);
    }

    //!< check if address is in IPv6 format
    else if(strchr(cmd, ':') != NULL)
    {
        //!< write the declared parameter to the address structure
        r = inet_pton(AF_INET6, address, &((struct sockaddr_in6*) &ss)->sin6_addr);
        ((struct sockaddr*) &ss)->sa_family = AF_INET6;
        ((struct sockaddr_in6*) &ss)->sin6_port = htons(port);
    }

    else
    {
        //!< if address is not in IPv4 or IPv6 format an error is returned
        return -1;
    }

    if(r != 1)
    {
        //!< is the address invalid -2 is returned
        return -2;
    }

    //!< the new contact is written to the connector pipe
    if((write(cnf->connect_fd[1], &ss, sizeof(ss)) == -1))
    {
        return -1;
    }

    return 0;
}


/*!function to show all commandos which are available*/
void parse_cmd_help(void)
{
    //!< print the available commands
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


/*! Used to list all informations about the contacts which are connected to dchat
 *  @param: cnf the global config structure
 *  @return: 0 if no error and -1 on error
 */
int parse_cmd_list(dchat_conf_t* cnf)
{
    int i, count = 0;
    char address[INET6_ADDRSTRLEN + 1];
    unsigned short port;
    struct sockaddr_storage* client_addr;

    for(i = 0; i < cnf->cl.cl_size; i++)
    {
        //!< check if entry is a valid connection
        if(cnf->cl.contact[i].fd)
        {
            client_addr = &cnf->cl.contact[i].stor;

            //!< check the version of the ip address
            if(ip_version(client_addr) == 4)
            {
                //!< get the address from the sockaddr_storage structure
                inet_ntop(AF_INET, &(((struct sockaddr_in*) client_addr)->sin_addr), address,
                          INET_ADDRSTRLEN);
                //!< get the port from the structure
                port = ntohs(((struct sockaddr_in*) client_addr)->sin_port);
            }

            else if(ip_version(client_addr) == 6)
            {
                //!< get the address from the sockaddr_storage structure
                inet_ntop(AF_INET, &(((struct sockaddr_in6*) client_addr)->sin6_addr), address,
                          INET_ADDRSTRLEN);
                //!< get the port from the structure
                port = ntohs(((struct sockaddr_in6*) client_addr)->sin6_port);
            }

            else
            {
                return -1;
            }

            //!< print all available information about the connection
            dprintf(cnf->out_fd, "\n\n"
                    "    Contact................%d\n"
                    "    Address-Tupel..........%s:%u\n"
                    "    Listening-Port.........%u\n"
                    "    Chatroom...............%d", i, address, port, cnf->cl.contact[i].lport,
                    cnf->cl.contact[i].chatrooms);
            count++;
        }
    }

    //!< are there no contacts in the list a message will be printed
    if(!count)
    {
        log_msg(LOG_INFO, "No contacts found in the contactlist");
    }

    return 0;
}


/*! this functions parse the user input, if there is a command in this input, the command
 *  will be executed
 *  @param cnf global config structure
 *  @param buf this string contains the user input
 *  @return 0 if the input was a command, -1 for error during parsen, 1 for program exit
 */
int parse_cmd(dchat_conf_t* cnf, char* buf)
{
    int ret;

    //check if the string contains the command /connect
    if(strncmp(buf, "/connect ", 9) == 0)
    {
        //!< to get the string without the command
        buf += 9;

        if((ret = parse_cmd_connect(cnf, buf)) == -1)
        {
            log_msg(LOG_WARN, "Wrong Syntax! Syntax of connect is: /connect <IP> <PORT>");
            return -2;
        }

        else if(ret == -2)
        {
            log_msg(LOG_WARN, "Wrong IP address format");
            return -2;
        }

        else if(ret == -3)
        {
            log_msg(LOG_WARN, "Invalid Portnumber");
            return -2;
        }
    }

    //!< check if the command contains exit and return 1
    else if(strcmp(buf, "/exit") == 0)
    {
        raise(SIGTERM);
    }

    //!< check for the command help and call the appropriate function
    else if(strcmp(buf, "/help") == 0)
    {
        parse_cmd_help();
    }

    //!< check for the command list and call the appropriate function
    else if(strcmp(buf, "/list") == 0)
    {
        parse_cmd_list(cnf);
    }

    else
    {
        return -1;
    }

    return 0;
}
