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


/** @file dchat.c
 *  This file is the main file for DChat and contains, besides the main function,
 *  several core functions. 
 *  
 *  Core functions are:
 *
 *  -) Handler for accepting connections
 *  
 *  -) Handler for user input
 *  
 *  -) Handler for remote data
 *  
 *  -) Handler for file sharing
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netinet/in.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <getopt.h>

#include "dchat_h/dchat.h"
#include "dchat_h/dchat_types.h"
#include "dchat_h/dchat_decoder.h"
#include "dchat_h/dchat_contact.h"
#include "dchat_h/dchat_cmd.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"


dchat_conf_t cnf;             //!< global dchat configuration structure
int ret_exit_ = EXIT_SUCCESS; //!< return value on termination


int
main(int argc, char** argv)
{
    int acpt_port   = DEFAULT_PORT; // local listening port
    int remote_port = DEFAULT_PORT; // port of remote client
    int option;                     // getopt option                     
    int required  = 0;              // counter for required options
    int found_int = 0;              // check existence of network int
    char* interface      = NULL;    // interface to bind to
    char* remote_address = NULL;    // ip of remote client
    char* nickname       = NULL;    // nickname to use within chat
    char* term;                     // used for strtol
    //FIXME: use sockaddr_storage for IP6 support
    struct sockaddr_in da;          // socket address for remote client
    struct sockaddr_storage sa;     // local socket address
    struct ifaddrs* ifaddr;         // info of all network interfaces
    struct ifaddrs* ifa;            // interface pointer
    //options for execution in terminal
    struct option long_options[] = 
    {
        {"interface", required_argument, 0, 'i'},
        {"lport", required_argument, 0, 'l'},
        {"nick", required_argument, 0, 'n'},
        {"dst", required_argument, 0, 'd'},
        {"rport", required_argument, 0, 'r'},
    };

    // parse commandline options
    while(1)
    {
        option = getopt_long(argc, argv, "i:l:n:d:r:", long_options, 0);  
        // end of options
        if(option == -1)
        {
            break;
        }

        switch(option)
        {
            case 'i':
                interface = optarg;
                required++;
            break;

            case 'l':
                acpt_port = (int) strtol(optarg, &term, 10);
                if(acpt_port < 0 || acpt_port > 65535 || *term != '\0'){
                    log_msg(LOG_ERR, "Invalid listening port '%s'", optarg);
                    usage();
                    return EXIT_FAILURE;
                }
            break;

            case 'n':
                nickname = optarg;
                required++;
            break;

            case 'd':
                remote_address = optarg;               
            break;

            case 'r':
                remote_port = (int) strtol(optarg, &term, 10);
                if(remote_port < 0 || remote_port > 65535 || *term != '\0'){
                    log_msg(LOG_ERR, "Invalid remote port '%s'", optarg);
                    usage();
                    return EXIT_FAILURE;
                }
            break;

            // invalid option - getopt prints error msg
            case '?':
            default:
                usage();
                return EXIT_FAILURE;
        }
    }
    
    // int and nick are mandatory; only options arguments are allowed
    if(required < 2 || optind < argc){
        usage();
        return EXIT_FAILURE;
    }

    if (getifaddrs(&ifaddr) == -1) 
    {
        log_msg(LOG_ERR, "main(): getifaddrs() failed"); 
        return (EXIT_FAILURE);
    }

    // iterate through network interfaces and set ip of interface
    memset(&sa, 0, sizeof(sa));
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        // FIXME: Support for IPv6
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET)
        {
            // is it the interface we are looking for?
            if (!strncmp(interface, ifa->ifa_name, strlen(ifa->ifa_name)))
            {
                if (ifa->ifa_addr != NULL)
                {
                    // temporarily store address
                    memcpy(&sa, ifa->ifa_addr, sizeof(struct sockaddr));
                    found_int = 1;    // we found the given interface
                    break;
                }
                else
                {
                    freeifaddrs(ifaddr);
                    log_msg(LOG_ERR,
                            "Interface '%s' does not have an ip address bound to it",
                            ifa->ifa_name);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    freeifaddrs(ifaddr);

    // network interface has not been found
    if (!found_int)
    {
        log_msg(LOG_ERR, "Interface '%s' not found", interface);
        usage();
        return EXIT_FAILURE;
    }

    // init dchat (e.g. global configuration, listening socket, threads, ...)
    if (init(&cnf, &sa, acpt_port, nickname) == -1)
    {
        return EXIT_FAILURE;
    }

    // has a remoteaddress been specified? if y: connect to it
    if (remote_address != NULL)
    {
        // init socket address structure
        memset(&da, 0, sizeof(da));
        da.sin_family = AF_INET;
        da.sin_port = htons(remote_port);

        // init ip address in socket address structure
        if (inet_pton(AF_INET, remote_address, &da.sin_addr) != 1)
        {
            log_errno(LOG_ERR, "wrong format of ip address");
        }

        // connect to remote client and send contactlist
        if (handle_local_conn_request(&cnf, (struct sockaddr_storage*) &da) ==
            -1)
        {
            log_errno(LOG_WARN,
                      "main(): Could not execute connection request successfully");
        }
    }
    
    // main chat loop
    ret_exit_ = th_main_loop(&cnf);
    // free resources and terminate this process
    raise(SIGTERM);
    return EXIT_SUCCESS;
}


/**
 * Initializes neccessary internal ressources like threads and pipes.
 * Initializes pipes and threads used for parallel processing of user input
 * and and handling data from a remote client. Furtermore the global config
 * gets initialized with the listening socket, nickname and other basic things.
 * @see init_global_config()
 * @param cnf The global dchat config
 * @param sa  Socket address containing the ip address to bind to
 * @param acpt_port Local listening port
 * @param nickname Nickname used for chatting
 * @return 0 on successful initialization, -1 in case of error
 */
int
init(dchat_conf_t* cnf, struct sockaddr_storage* sa, int acpt_port, char* nickname)
{
    struct sigaction sa_terminate; // signal action for program termination
    sa_terminate.sa_handler = terminate;
    sigemptyset(&sa_terminate.sa_mask);
    sa_terminate.sa_flags = 0;
    sigaction(SIGHUP, &sa_terminate, NULL);   // terminal line hangup
    sigaction(SIGQUIT, &sa_terminate, NULL);  // quit programm
    sigaction(SIGINT, &sa_terminate, NULL);   // interrupt programm
    sigaction(SIGTERM, &sa_terminate, NULL);  // software termination

    // check if given port is valid
    if (acpt_port < 1 || acpt_port > 65535)
    {
        log_msg(LOG_WARN, "Invalid port for listening socket");
        return -1;
    }

    // init the listening socket
    if (init_global_config(cnf, sa, acpt_port, nickname) == -1)
    {
        return -1;
    }

    // pipe to the th_new_conn
    if (pipe(cnf->connect_fd) == -1)
    {
        log_errno(LOG_ERR, "could not create pipe");
        return -1;
    }

    // pipe to send signal to wait loop from connect
    if (pipe(cnf->cl_change) == -1)
    {
        log_errno(LOG_ERR, "could not create cl_change pipe");
        return -1;
    }

    // pipe to signal new user input from stdin
    if (pipe(cnf->user_input) == -1)
    {
        log_errno(LOG_ERR, "could not create cl_change pipe");
        return -1;
    }

    // init the mutex function used for locking the contactlist
    if (pthread_mutex_init(&cnf->cl.cl_mx, NULL))
    {
        log_errno(LOG_ERR, "pthread_mutex_init() failed");
        return -1;
    }

    // create new th_new_conn-thread
    if (pthread_create
        (&cnf->conn_th, NULL, (void* (*)(void*)) th_new_conn, cnf) == -1)
    {
        log_errno(LOG_ERR, "pthread_create() failed");
        return -1;
    }

    // create new thread for handling userinput from stdin
    if (pthread_create
        (&cnf->userin_th, NULL, (void* (*)(void*)) th_new_input, cnf) == -1)
    {
        log_errno(LOG_ERR, "pthread_create() failed");
        return -1;
    }

    return 0;
}


/**
 * Initializes the global dchat configuration.
 * Binds to a network interface, creates a listening socket for this interface
 * and the given port and sets the nickname. All configuration settings will
 * be stored in the global config.
 * @param cnf Pointer to global configuration structure
 * @param sa  Socket address containing the ip address to bind to
 * @param acpt_port Port to bind to and listen for incoming connetion requests
 * @param nickname Nickname used for this chat session
 * @return socket descriptor or -1 if an error occurs
 */
int
init_global_config(dchat_conf_t* cnf, struct sockaddr_storage* sa, int acpt_port,
                   char* nickname)
{
    char addr_str[INET6_ADDRSTRLEN + 1];  // ip as string
    int s;                    // local socket descriptor
    int on = 1;               // turn on a socket option
    int found_int = 0;        // check if interface was found

    // create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        log_errno(LOG_ERR, "socket() failed");
        return -1;
    }

    // socketoption to prevent: 'bind() address already in use'
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        log_errno(LOG_ERR, "setsockopt() failed");
        close(s);
        return -1;
    }

    // add port to socketaddress structure
    if (ip_version(sa) == 4)
    {
        ((struct sockaddr_in*) sa)->sin_port = htons(acpt_port);
    }
    else if (ip_version(sa) == 6)
    {
        ((struct sockaddr_in6*) sa)->sin6_port = htons(acpt_port);
    }

    // bind socket to a socket address
    if (bind(s, (struct sockaddr*) sa, sizeof(struct sockaddr)) == -1)
    {
        log_errno(LOG_ERR, "bind() failed");
        close(s);
        return -1;
    }

    // listen on this socket
    if (listen(s, 5) == -1)
    {
        log_errno(LOG_ERR, "listen() failed");
        close(s);
        return -1;
    }

    // log, where on which address and port this client will be
    // listening
    if (ip_version(sa) == 4)
    {
        log_msg(LOG_INFO, "Listening on '%s:%d'",
                inet_ntop(AF_INET, &((struct sockaddr_in*) sa)->sin_addr,
                          addr_str, INET_ADDRSTRLEN), acpt_port);
    }
    else if (ip_version(sa) == 6)
    {
        log_msg(LOG_INFO, "Listening on '%s:%d'",
                inet_ntop(AF_INET6, &((struct sockaddr_in6*) sa)->sin6_addr,
                          addr_str, INET6_ADDRSTRLEN), acpt_port);
    }

    // init the global config structure
    memset(cnf, 0, sizeof(*cnf));
    cnf->in_fd = 0;            // use stdin as input source
    cnf->out_fd = 1;           // use stdout as output source
    cnf->cl.cl_size = 0;       // set initial size of contactlist
    cnf->cl.used_contacts = 0; // no known contacts, at start
    cnf->me.name[0] = '\0';
    strncat(cnf->me.name, nickname, MAX_NICKNAME); // set nickname
    cnf->me.lport = acpt_port; // set listening-port
    // set socket address, where this client is listening
    memcpy(&cnf->me.stor, sa, sizeof(struct sockaddr_storage));
    cnf->acpt_fd = s; // set socket file descriptor
    return s;
}


/**
 * Frees all global config resources used.
 * Closes all open file descriptors, sockets, pipes and mutexes stored within
 * the global config.
 * @param cnf Global dchat config
 */
void
destroy(dchat_conf_t* cnf)
{
    int i;

    // close file descriptors of contacts
    for (i = 0; i < cnf->cl.cl_size; i++)
    {
        if (cnf->cl.contact[i].fd)
        {
            close(cnf->cl.contact[i].fd);
        }
    }

    // close write pipe for thread function th_new_conn
    close(cnf->connect_fd[1]);
    // close write pipe for main thread function th_main_loop
    close(cnf->cl_change[1]);
    // close write pipe for thread function th_new_input
    close(cnf->user_input[1]);
    // close local listening socket
    close(cnf->acpt_fd);
    // destroy contact mutex
    pthread_mutex_destroy(&cnf->cl.cl_mx);
}


/**
 * Signal handler function used for termination.
 * Signal handler function that will free all used resources like file
 * descriptors, pipes, etc... and that terminates this process afterwards.
 * This function will be called whenever this program should terminate (e.g
 * SIGTERM, SIGQUIT, ...) or on EOF of stdin.
 * Exit status of the process will be 0 on success, or a non-zero value
 * in case of error.
 * @see destroy()
 * @param sig Type of signal (e.g SIGTERM, SIGQUIT, ...)
 */
void
terminate(int sig)
{
    // free all resources used like file descriptors, sockets, ...
    destroy(&cnf);
    // delete readline prompt and return to beginning of current line
    ansi_term_clear_line(cnf.out_fd);
    ansi_term_cr(cnf.out_fd);
    dprintf(cnf.out_fd, "Good Bye!\n");
    // terminate process
    ret_exit_ == 0 ? exit(EXIT_SUCCESS) : exit(EXIT_FAILURE);
}


/**
 * Handles local user input.
 * Interpretes the given line and reacts correspondently to it. If the
 * line is a command it will be executed, otherwise it will be treated as
 * text message and send to all known contacts stored in the contactlist
 * in the global configuration.
 * @see write_pdu()
 * @param cnf Pointer to global configuration
 * @return -2 on warning, -1 on error, 0 on success
 */
int
handle_local_input(dchat_conf_t* cnf, char* line)
{
    dchat_pdu_t msg; // pdu containing the chat text message
    int i, ret = 0, len, cmd;

    // user entered command
    if ((cmd = parse_cmd(cnf, line)) == 0)
    {
        return 0;
    }
    // command has wrong syntax
    else if (cmd == -2)
    {
        log_msg(LOG_WARN, "Syntax error");
        return -2;
    }
    // no command has been entered
    else
    {
        len = strlen(cnf->me.name) + 2;  // memory for local nickname +2 for ": "
        len += strlen(line);             // memory for text message

        if (len != 0)
        {
            // init dchat pdu
            memset(&msg, 0, sizeof(msg));
            msg.content_type = CT_TXT_PLAIN;
            msg.listen_port = cnf->me.lport;

            // allocate memory the size of nickname + 2 + text message
            if ((msg.content = malloc(len + 1)) == NULL)
            {
                log_errno(LOG_ERR,
                          "handle_local_input() failed, could not allocated memory");
                return -1;
            }

            // append the content
            msg.content[0] = '\0';
            strncat(msg.content, cnf->me.name, strlen(cnf->me.name));
            strncat(msg.content, ": ", 2);
            strncat(msg.content, line, strlen(line));
            msg.content_length = len; // length of content excluding \0

            // write pdu to known contacts
            for (i = 0; i < cnf->cl.cl_size; i++)
            {
                if (cnf->cl.contact[i].fd)
                {
                    ret = write_pdu(cnf->cl.contact[i].fd, &msg);
                }
            }

            free(msg.content);
        }
        else
        {
            log_msg(LOG_WARN, "handle_local_input got empty string");
        }
    }

    // return value of write_pdu
    return ret != -1 ? 0 : -1;
}


/**
 * Handles PDUs received from a remote client.
 * Reads in a PDU from a certain contact file descriptor and interpretes its
 * headers and handles its content.
 * @param cnf Pointer to global configuration holding the contactlist
 * @param n Index of contact in the respective contactlist
 * @return length of bytes read, 0 on EOF or -1 in case of error
 */
int
handle_remote_input(dchat_conf_t* cnf, int n)
{
    dchat_pdu_t* pdu; // pdu read from contact file descriptor
    char* txt_msg;    // message used to store remote input
    int ret;          // return value
    int len;          // amount of bytes read
    int fd;           // file descriptor of a contact
    fd = cnf->cl.contact[n].fd;

    // read pdu from file descriptor (-1 indicates error)
    if ((len = read_pdu(fd, &pdu)) == -1)
    {
        log_msg(LOG_ERR, "Illegal PDU from contact %d", n);
        return -1;
    }
    // EOF
    else if (!len)
    {
        log_msg(LOG_INFO, "contact '%d' disconnected", n);
        return 0;
    }

    // the first pdu of a newly connected client has to be a
    // "control/discover" otherwise raise an error and delete
    // this contact
    if (!cnf->cl.contact[n].lport && pdu->content_type != CT_CTRL_DISC)
    {
        log_msg(LOG_ERR, "Contact %d has not identfied himself", n);
        return -1;
    }
    else if (!cnf->cl.contact[n].lport && pdu->content_type == CT_CTRL_DISC)
    {
        // set listening port of contact
        cnf->cl.contact[n].lport = pdu->listen_port;
    }

    /*
     * == TEXT/PLAIN ==
     */
    if (pdu->content_type == CT_TXT_PLAIN)
    {
        // allocate memory for text message
        if ((txt_msg = malloc(pdu->content_length + 1)) == NULL)
        {
            log_errno(LOG_ERR,
                      "handle_remote_input() failed - Could not allocate memory");
            free_pdu(pdu);
            return -1;
        }

        // store bytes from pdu in txt_msg and terminate it
        memcpy(txt_msg, pdu->content, pdu->content_length);
        txt_msg[pdu->content_length] = '\0';
        // print text message
        print_dchat_msg(txt_msg, cnf->out_fd);
        free(txt_msg);
    }
    /*
     * == CONTROL/DISCOVER ==
     */
    else if (pdu->content_type == CT_CTRL_DISC)
    {
        // since dchat brings with the problem of duplicate contacts
        // check if there are duplicate contacts in the contactlist
        if ((ret = check_duplicates(cnf, n)) == -2)
        {
            // ERROR
            log_msg(LOG_ERR, "check_duplicate_contact() failed");
        }
        // no duplicates
        else if (ret == -1)
        {
            // log_msg(LOG_INFO, "no duplicates founds");
        }
        // duplicates found
        else
        {
            log_msg(LOG_INFO, "Duplicate detected - Removing it (%d)", ret);
            del_contact(cnf, ret);  // delete duplicate
        }

        // iterate through the content of the pdu containing
        // the new contacts
        if ((ret = receive_contacts(cnf, pdu)) == -1)
        {
            log_msg(LOG_WARN,
                    "handle_remote_input() - 'control/discover' failed:  Not every new contact could be added");
        }
        else if (ret == 0)
        {
            // log_msg(LOG_INFO, "client %d does not know any other
            // contacts we do not know about", n);
        }
        else
        {
            // log_msg(LOG_INFO, "%d new client(s) have been added",
            // ret);
        }
    }
    /*
     * == UNKNOWN CONTENT-TYPE ==
     */
    else
    {
        log_msg(LOG_WARN,
                "handle_remote_input(): Content-Type not implemented yet");
    }

    free_pdu(pdu);
    return len;
}


/**
 * Handles local connection requests from the user.
 * Connects to the remote client with the given socket address, who will
 * be added as contact. This new contact will be sent that all of our known
 * contacts as specified in the DChat protocol.
 * @param cnf Pointer to global configuration holding the contactlist
 * @param da Destination address to connect to
 * @return The index where the contact has been added in the contactlist,
 * -1 on error
 */
int
handle_local_conn_request(dchat_conf_t* cnf, struct sockaddr_storage* da)
{
    int s; // socket of the contact we have connected to
    int n; // index of the contact in our contactlist

    // connect to given address
    if ((s = connect_to((struct sockaddr*) da)) == -1)
    {
        return -1;
    }
    else
    {
        // add contact
        if ((n = add_contact(cnf, s, da)) == -1)
        {
            log_errno(LOG_ERR,
                      "handle_local_conn_request() failed - Could not add contact");
            return -1;
        }
        else
        {
            // set listening port of contact (we now it, since we
            // connected to it)
            if (ip_version(da) == 4)
            {
                cnf->cl.contact[n].lport =
                    ntohs(((struct sockaddr_in*) da)->sin_port);
            }
            else if (ip_version(da) == 6)
            {
                cnf->cl.contact[n].lport =
                    ntohs(((struct sockaddr_in6*) da)->sin6_port);
            }

            // send all our known contacts to the newly connected client
            send_contacts(cnf, n);
        }
    }

    return n;
}


/**
 * Handles connection requests from a remote client.
 * Accepts a connection from a remote client and so that a new TCP connection
 * will be established between this and the remote client. Moreover the remote
 * client will be added as new contact in the contactlist.
 * @see add_contact()
 * @param cnf Pointer to global configuration
 * @return return value of function add_contact, or -1 on error
 */
int
handle_remote_conn_request(dchat_conf_t* cnf)
{
    int s;                      // socket file descriptor
    int n;                      // index of new contact
    struct sockaddr_storage ss; // socketaddress of new contact
    socklen_t socklen;          // length of socketaddress
    socklen = sizeof(ss);

    // accept connection request
    if ((s = accept(cnf->acpt_fd, (struct sockaddr*) &ss, &socklen)) == -1)
    {
        log_errno(LOG_ERR, "accept() failed");
        return -1;
    }

    // add new contact to contactlist
    n = add_contact(cnf, s, &ss);

    if (n != -1)
    {
        log_msg(LOG_INFO, "contact %d connected", n);
    }

    return n;
}


/**
 * Thread function that reads a socket address from a pipe and connects to this
 * address.
 * Establishes new connections to the given addresses read from the global config
 * pipe `connect_fd`. It reads a sockaddr_storage structure and then connects to
 * this socket address. If a connection has been established successfully, a new
 * contact will be added and our contactlist will be sent to him. Furthermore
 * the character '1' will be written to the global config pipe `cl_change`.
 * @see handle_local_conn_request()
 * @param cnf: Pointer to global config to read from the pipe connect_fd`
 */
void*
th_new_conn(dchat_conf_t* cnf)
{
    struct sockaddr_storage da; // destination address to connect to
    char c = '1';               // will be written to `cl_change`
    int ret;

    for (;;)
    {
        // read from pipe to get new addresses
        if ((ret = read(cnf->connect_fd[0], &da, sizeof(da))) == -1)
        {
            log_msg(LOG_WARN, "th_new_conn(): could not read from pipe");
        }

        // EOF
        if (!ret)
        {
            close(cnf->connect_fd[0]);
            close(cnf->cl_change[1]);
            break;
        }

        // lock contactlist
        pthread_mutex_lock(&cnf->cl.cl_mx);

        if (handle_local_conn_request(cnf, &da) == -1)
        {
            log_errno(LOG_WARN,
                      "th_new_conn(): Could not execute connection request successfully");
        }
        else if ((write(cnf->cl_change[1], &c, sizeof(c))) == -1)
        {
            log_msg(LOG_WARN, "th_new_conn(): could not write to pipe");
        }

        // unlock contactlist
        pthread_mutex_unlock(&cnf->cl.cl_mx);
    }

    pthread_exit(NULL);
}


/**
 * Thread function that reads from stdin until the user hits enter.
 * Waits for new user input on stdin using GNU readline. If the user has
 * entered something, it will be written to the global config pipe `user_input`.
 * First the length (int) of the string will be written to the pipe and then each
 * byte of the string. On EOF of stdin 0 will be written
 * @param cnf: Pointer to global config to write to the pipe `user_input`
 */
void*
th_new_input(dchat_conf_t* cnf)
{
    char prompt[MAX_NICKNAME + 8]; // readline prompt contains nickname
    char* line;                    // line read from stdin
    int len;                       // length of line
    // Assemble prompt for GNU readline
    prompt[0] = '\0';
    strncat(prompt, "Me (", 4);
    strncat(prompt, cnf->me.name, MAX_NICKNAME);
    strncat(prompt, "): ", 3);

    while (1)
    {
        // wait until user entered a line that can be read from stdin
        line = readline(prompt);

        // EOF or user has entered "/exit"
        if (line == NULL || !strcmp(line, "/exit"))
        {
            // since EOF is not a signal - raise SIGTERM
            // to terminate this process
            raise(SIGTERM);
            break;
        }
        else
        {
            len = strlen(line);

            // user did not write anything -> just hit enter
            if (len == 0)
            {
                len = 1;
                write(cnf->user_input[1], &len, sizeof(int));
                write(cnf->user_input[1], "\n", len);
            }
            else
            {
                // first write length of string
                write(cnf->user_input[1], &len, sizeof(int));
                // then write the bytes of the string
                write(cnf->user_input[1], line, len);
            }

            free(line);
        }
    }

    pthread_exit(NULL);
}


/**
 * Main chat loop of this client.
 * This function is the main loop of DChat that selects(2) certain file
 * descriptors stored in the global configuration to read from.
 * It waits for local userinput, PDUs from remote clients, local connection
 * requests and remote connection requests. If select returns and a  file
 * descriptor can be read, this function will take action depending on which file
 * descriptor is able to read from.
 * @param cnf Pointer to global config holding all kind of file descriptors
 * @return 0 or -1 in case of error
 */
int
th_main_loop(dchat_conf_t* cnf)
{
    fd_set rset; // list of readable file descriptors
    int nfds;    // number of fd in rset
    int ret;     // return value
    char c;      // for pipe: th_new_conn
    char* line;  // line returned from GNU readline
    int i;

    for (;;)
    {
        // INIT fd_set
        FD_ZERO(&rset);
        // ADD STDIN: pipe file descriptor that connects the thread
        // function 'th_new_input'
        FD_SET(cnf->user_input[0], &rset);
        // ADD LISTENING PORT: acpt_fd of global config
        FD_SET(cnf->acpt_fd, &rset);
        nfds = max(cnf->user_input[0], cnf->acpt_fd);
        // ADD NEW CONN: pipe file descriptor that connects the thead
        // function 'th_new_conn'
        FD_SET(cnf->cl_change[0], &rset);
        nfds = max(nfds, cnf->cl_change[0]);
        // ADD CONTACTS: add all contact socket file descriptors from
        // the contactlist
        pthread_mutex_lock(&cnf->cl.cl_mx);

        for (i = 0; i < cnf->cl.cl_size; i++)
        {
            if (cnf->cl.contact[i].fd)
            {
                FD_SET(cnf->cl.contact[i].fd, &rset);
                // determine fd with highest value
                nfds = max(cnf->cl.contact[i].fd, nfds);
            }
        }

        pthread_mutex_unlock(&cnf->cl.cl_mx);
        // used as backup since nfds will be overwritten if an
        // interrupt
        // occurs
        int old_nfds = nfds;

        while ((nfds = select(old_nfds + 1, &rset, NULL, NULL, NULL)) == -1)
        {
            // something interrupted select(2) - try select again
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                log_errno(LOG_ERR, "select() failed");
                return -1;
            }
        }

        // CHECK STDIN: check if thread has written to the user_input
        // pipe
        if (FD_ISSET(cnf->user_input[0], &rset))
        {
            nfds--;
            // read length of string from pipe
            read(cnf->user_input[0], &ret, sizeof(int));

            // EOF
            if (!ret)
            {
                close(cnf->user_input[0]);
                return 0;
            }

            // allocate memory for the string entered from user
            line = malloc(ret + 1);
            // read string
            read(cnf->user_input[0], line, ret);
            line[ret] = '\0';
            pthread_mutex_lock(&cnf->cl.cl_mx);

            // handle user input
            if ((ret = handle_local_input(cnf, line)) == -1)
            {
                return -1;
            }

            pthread_mutex_unlock(&cnf->cl.cl_mx);
            free(line);
        }

        // CHECK LISTENING PORT: check if new connection can be
        // accepted
        if (FD_ISSET(cnf->acpt_fd, &rset))
        {
            nfds--;
            pthread_mutex_lock(&cnf->cl.cl_mx);

            // handle new connection request
            if ((ret = handle_remote_conn_request(cnf)) == -1)
            {
                return -1;
            }
            else
            {
                send_contacts(cnf, ret);
            }

            pthread_mutex_unlock(&cnf->cl.cl_mx);
        }

        // CHECK NEW CONN: check if user new connection has been added
        if (FD_ISSET(cnf->cl_change[0], &rset))
        {
            nfds--;

            // !< EOF
            if (!read(cnf->cl_change[0], &c, sizeof(c)))
            {
                close(cnf->cl_change[0]);
                return 0;
            }
        }

        // CHECK CONTACTS: check file descriptors of contacts
        // check if nfds is 0 => no contacts have written something
        pthread_mutex_lock(&cnf->cl.cl_mx);

        for (i = 0; nfds && i < cnf->cl.cl_size; i++)
        {
            if (FD_ISSET(cnf->cl.contact[i].fd, &rset))
            {
                nfds--;

                // handle input from remote user
                // -1 = error, 0 = EOF
                if ((ret = handle_remote_input(cnf, i)) == -1 || ret == 0)
                {
                    close(cnf->cl.contact[i].fd);
                    del_contact(cnf, i);
                }
            }
        }

        pthread_mutex_unlock(&cnf->cl.cl_mx);
    }
}


/**
 * Prints usage of this program.
 */
void
usage()
{
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "  %s  -i  INTERFACE  -n  NICKNAME  [-l  LOCALPORT]  [-d  REMOTEIP] [-r  REMOTEPORT]\n\n", PACKAGE_NAME);
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  -i, --interface=INTERFACE\n");
    fprintf(stdout, "  -n, --nick=NICKNAME\n");
    fprintf(stdout, "  -l, --lport=LOCALPORT\n");
    fprintf(stdout, "  -d, --dst=REMOTEIP\n");
    fprintf(stdout, "  -r, --rport=REMOTEPORT\n");
    fprintf(stdout, "\nMore detailed information can be found in the manpage. See dchat(1)");
}
