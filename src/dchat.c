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
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include "dchat_h/dchat.h"
#include "dchat_h/types.h"
#include "dchat_h/decoder.h"
#include "dchat_h/contact.h"
#include "dchat_h/cmdinterpreter.h"
#include "dchat_h/network.h"
#include "dchat_h/util.h"
#include "dchat_h/option.h"


#include "dchat_h/consoleui.h"

dchat_conf_t config;
dchat_conf_t* _cnf = &config;


int
main(int argc, char** argv)
{
    char option[2];                     // getopt option
    int required = 0;                   // counter for required options
    int required_set = 0;               // counter for set required options
    char* short_opts   = NULL;          // gnu getopt string
    struct option* long_opts;           // gnu getopt long options
    int found_opt;                      // boolean if opt has been found
    cli_options_t options;              // available command line options
    char* remote_onion = NULL;          // onion id of remote host
    int rport;                          // remote port
    int ret;

    if (init_global_config() == -1)
    {
        ui_fatal("Initialization of global configuration failed!");
    }

    if (init_cli_options(&options) == -1)
    {
        ui_fatal("Initialization of command line options failed!");
    }

    short_opts = get_short_options(&options);
    long_opts = get_long_options(&options);

    // deterime amount of required options
    for (int i = 0; i < CLI_OPT_AMOUNT; i++)
    {
        if (options.option[i].mandatory_option)
        {
            required++;
        }
    }

    while (1)
    {
        ret = getopt_long(argc, argv, short_opts, long_opts, 0);

        // end of options
        if (ret == -1)
        {
            break;
        }

        option[0] = ret;
        option[1] = '\0';
        // iterate available options
        found_opt = 0;

        for (int i = 0; i < CLI_OPT_AMOUNT; i++)
        {
            if (!strncmp(&options.option[i].opt, option, 1))
            {
                found_opt = 1;

                if ((ret = options.option[i].parse_option(optarg, 1)) == -1)
                {
                    usage(EXIT_FAILURE, &options, "Invalid argument '%s' for option '-%c / --%s'",
                          optarg, options.option[i].opt, options.option[i].long_opt);
                }

                // increment counter of set required options
                // if the parsing function has set the options value
                // in the global conf
                if (options.option[i].mandatory_option && !ret)
                {
                    required_set++;
                }
            }
        }

        if (!found_opt)
        {
            usage(EXIT_FAILURE, &options, "Invalid command-line option!");
        }
    }

    if (short_opts != NULL)
    {
        free(short_opts);
    }

    if (long_opts != NULL)
    {
        free(long_opts);
    }

    // read configuration file if existent and set corresponding
    // values in the global config
    if ((ret = file_exists(CONFIG_PATH)) == 1)
    {
        if ((ret = read_conf(CONFIG_PATH, &required_set)) == -1)
        {
            ui_log(LOG_WARN, "Reading configuration file failed!");
        }

        if (ret > 0)
        {
            ui_log(LOG_WARN, "Syntax error in line '%d' of config file!", ret);
        }
    }
    else if (ret == -1)
    {
        ui_log_errno(LOG_WARN, "Could not read configuration file '%s'!", CONFIG_PATH);
    }

    // check if all required options have been specified
    if (required != required_set)
    {
        usage(EXIT_FAILURE, &options, "Missing mandatory command-line options!");
    }

    // client requires no arguments -> raise error if there are any
    if (optind < argc)
    {
        usage(EXIT_FAILURE, &options, "Invalid command-line arguments!");
    }

    // create listening socket
    if (init_listening(LISTEN_ADDR) == -1)
    {
        ui_fatal("Initialization of listening socket failed!");
    }

    if (_cnf->cl.used_contacts == 1)
    {
        remote_onion = _cnf->cl.contact[0].onion_id;
        rport = _cnf->cl.contact[0].lport;
    }

    // init threads (connection thread, userinput thread, ...)
    if (init_threads(&_cnf) == -1)
    {
        ui_fatal("Initialization of threads failed!");
    }

    // has a remote onion address or remote port been specified? (check
    // fake contact - see: roni_parse() / rprt_parse()) if y: connect to
    // it
    if (_cnf->cl.used_contacts == 1)
    {
        // use default if onion-id has not been specified
        if (is_valid_onion(_cnf->cl.contact[0].onion_id))
        {
            remote_onion = _cnf->cl.contact[0].onion_id;
        }
        else
        {
            remote_onion = _cnf->me.onion_id;
        }

        // use default if remote port has not been specified
        if (is_valid_port(_cnf->cl.contact[0].lport))
        {
            rport = _cnf->cl.contact[0].lport;
        }
        else
        {
            rport = DEFAULT_PORT;
        }

        // delete fake contact
        del_contact(0);
        // inform connection handler to connect to the specified
        // remote host
        write(_cnf->connect_fd[1], remote_onion, ONION_ADDRLEN);
        write(_cnf->connect_fd[1], &rport,       sizeof(uint16_t));
    }

    if (init_ui() == -1)
    {
        ui_fatal("Initialization of user interface failed!");
    }

    // handle userinput
    ret = th_new_input();
    // cleanup all ressources
    destroy();
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}


/**
 * Initializes basic fields of  the global dchat configuration.
 * Initial values of the contactlist will be set.
 * @return 0 on success, -1 in case of error
 */
int
init_global_config()
{
    memset(_cnf, 0, sizeof(*_cnf));
    _cnf->cl.cl_size       = 0;    // set initial size of contactlist
    _cnf->cl.used_contacts = 0;    // no known contacts, at start
    return 0;
}


/**
 * Initializes the listening socket.
 * Binds to a socket address, creates a listening socket for this interface
 * and the given port stored in the global dchat config.
 * @param address IP Address for listening
 * @return socket descriptor or -1 if an error occurs
 */
int
init_listening(char* address)
{
    int s;      // local socket descriptor
    int on = 1; // turn on a socket option
    struct sockaddr_storage sa;
    // setup local listening socket address
    memset(&sa, 0, sizeof(sa));

    if (inet_pton(AF_INET, address, &((struct sockaddr_in*)&sa)->sin_addr) != 1)
    {
        ui_log(LOG_ERR, "Invalid listening ip address '%s'", address);
        return -1;
    }

    ((struct sockaddr_in*)&sa)->sin_family = AF_INET;

    if (!_cnf->me.lport)
    {
        _cnf->me.lport = DEFAULT_PORT;
        ((struct sockaddr_in*)&sa)->sin_port = htons(DEFAULT_PORT);
    }
    else
    {
        ((struct sockaddr_in*)&sa)->sin_port = htons(_cnf->me.lport);
    }

    // create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        ui_log_errno(LOG_ERR, "Creation of socket failed!");
        return -1;
    }

    // socketoption to prevent: 'bind() address already in use'
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        ui_log_errno(LOG_ERR,
                     "Setting socket options to reuse an already bound address failed!");
        close(s);
        return -1;
    }

    // bind socket to a socket address
    if (bind(s, (struct sockaddr*) &sa, sizeof(struct sockaddr)) == -1)
    {
        ui_log_errno(LOG_ERR, "Binding to socket address failed!");
        close(s);
        return -1;
    }

    // listen on this socket
    if (listen(s, LISTEN_BACKLOG) == -1)
    {
        ui_log_errno(LOG_ERR, "Listening on socket descriptor failed!");
        close(s);
        return -1;
    }

    // set socket address, where this client is listening
    memcpy(&_cnf->sa, &sa, sizeof(struct sockaddr_storage));
    _cnf->acpt_fd = s;          // set socket file descriptor
    return s;
}


/**
 * Initializes neccessary internal ressources like threads and pipes.
 * Initializes pipes and threads used for parallel processing of user input
 * and handling data from a remote client and installs a signal handler to
 * catch basic termination signals for proper program termination.
 * @return 0 on successful initialization, -1 in case of error
 */
int
init_threads()
{
    struct sigaction sa_terminate; // signal action for program termination
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGHUP);
    sigaddset(&sigmask, SIGQUIT);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGTERM);
    // all threads should block the following signals
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
    sa_terminate.sa_handler = terminate;
    // do not allow interruption of a signal handler
    sa_terminate.sa_mask = sigmask;
    sa_terminate.sa_flags = 0;
    sigaction(SIGHUP,  &sa_terminate, NULL); // terminal line hangup
    sigaction(SIGQUIT, &sa_terminate, NULL); // quit programm
    sigaction(SIGINT,  &sa_terminate, NULL); // interrupt programm
    sigaction(SIGTERM, &sa_terminate, NULL); // software termination

    // pipe to the th_new_conn
    if (pipe(_cnf->connect_fd) == -1)
    {
        ui_log_errno(LOG_ERR, "Creation of connection pipe failed!");
        return -1;
    }

    // pipe to send signal to wait loop from connect
    if (pipe(_cnf->cl_change) == -1)
    {
        ui_log_errno(LOG_ERR, "Creation of change pipe failed!");
        return -1;
    }

    // pipe to signal new user input from stdin
    if (pipe(_cnf->user_input) == -1)
    {
        ui_log_errno(LOG_ERR, "Creation of userinput pipe faild!");
        return -1;
    }

    // init the mutex function used for locking the contactlist
    if (pthread_mutex_init(&_cnf->cl.cl_mx, NULL))
    {
        ui_log_errno(LOG_ERR, "Initialization of mutex failed!");
        return -1;
    }

    // create new th_new_conn-thread
    if (pthread_create
        (&_cnf->conn_th, NULL, (void* (*)(void*)) th_new_conn, _cnf) == -1)
    {
        ui_log_errno(LOG_ERR, "Creation of connection thread failed!");
        return -1;
    }

    // create new thread for handling userinput from stdin
    if (pthread_create
        (&_cnf->select_th, NULL, (void* (*)(void*)) th_main_loop, _cnf) == -1)
    {
        ui_log_errno(LOG_ERR, "Creation of selection thread failed!");
        return -1;
    }

    // main thread should not block any signals
    pthread_sigmask(SIG_UNBLOCK, &sigmask, NULL);
    return 0;
}


/**
 * Frees all global config resources used.
 * Closes all open file descriptors, sockets, pipes and mutexes stored within
 * the global config.
 */
void
destroy()
{
    // cancel select thread
    pthread_cancel(_cnf->select_th);
    // wait for termination of select thread
    pthread_join(_cnf->select_th, NULL);
    // cancel connection thread
    pthread_cancel(_cnf->conn_th);
    // cancel connection thread
    // wait for termination of connection thread
    pthread_join(_cnf->conn_th, NULL);
    // wait for termination of select thread
    // destroy contact mutex
    pthread_mutex_destroy(&_cnf->cl.cl_mx);
    // close write pipe for thread function th_new_conn
    close(_cnf->connect_fd[1]);
    // close write pipe for thread function th_new_input
    close(_cnf->user_input[1]);
    // delete readline prompt and return to beginning of current line
    local_log(LOG_INFO, "Good Bye!");
}


/**
 * Signal handler function used for termination.
 * Signal handler function that will free all used resources like file
 * descriptors, pipes, etc. and that terminates this process afterwards.
 * This function will be called whenever this program should terminate (e.g
 * SIGTERM, SIGQUIT, ...) or on EOF of stdin.
 * Exit status of the process will be EXIT_SUCCESS
 * @see destroy()
 * @param sig Type of signal (e.g SIGTERM, SIGQUIT, ...)
 */
void
terminate(int sig)
{
    // free all resources used like file descriptors, sockets, ...
    destroy();
    exit(EXIT_SUCCESS);
}


/**
 * Handles local user input.
 * Interpretes the given line and reacts correspondently to it. If the
 * line is a command it will be executed, otherwise it will be treated as
 * text message and send to all known contacts stored in the contactlist
 * in the global configuration.
 * @see write_pdu()
 * @return 0 on success, -1 on error
 */
int
handle_local_input(char* line)
{
    dchat_pdu_t msg; // pdu containing the chat text message
    int i, ret = 0, len;

    // check if user entered command
    if ((ret = parse_cmd(line)) == 0 || ret == 1)
    {
        return 0;
    }
    // no command has been entered / or command could not be processed
    else
    {
        ret = 0;
        len = strlen(line); // memory for text message

        if (len != 0)
        {
            // inititialize pdu
            if (init_dchat_pdu(&msg, CTT_ID_TXT, 1.0, _cnf->me.onion_id, _cnf->me.lport,
                               _cnf->me.name) == -1)
            {
                ui_log(LOG_ERR, "Initialization of PDU failed!");
                return -1;
            }

            // set content of pdu
            init_dchat_pdu_content(&msg, line, strlen(line));

            // write pdu to known contacts
            for (i = 0; i < _cnf->cl.cl_size; i++)
            {
                if (_cnf->cl.contact[i].fd)
                {
                    ret = write_pdu(_cnf->cl.contact[i].fd, &msg);
                }
            }

            free_pdu(&msg);
        }
    }

    // return value of write_pdu
    return ret != -1 ? 0 : -1;
}


/**
 * Handles PDUs received from a remote client.
 * Reads in a PDU from a certain contact file descriptor and interpretes its
 * headers and handles its content.
 * @param n Index of contact in the respective contactlist
 * @return length of bytes read, 0 on EOF or -1 in case of error
 */
int
handle_remote_input(int n)
{
    dchat_pdu_t pdu;    // pdu read from contact file descriptor
    char* txt_msg;      // message used to store remote input
    int ret;            // return value
    int len;            // amount of bytes read
    contact_t* contact; // contact to send a message to
    contact = &_cnf->cl.contact[n];

    // read pdu from file descriptor (-1 indicates error)
    if ((len = read_pdu(contact->fd, &pdu)) == -1)
    {
        ui_log(LOG_ERR, "Illegal PDU from '%s'!", contact->name);
        return -1;
    }
    // EOF
    else if (!len)
    {
        ui_log(LOG_INFO, "'%s' disconnected!", contact->name);
        return 0;
    }

    // the first pdus of a newly connected client have to be a
    // "control/discover" containing the onion-id and listening
    // port, otherwise raise an error and delete
    // this contact
    if ((contact->onion_id[0] == '\0' || !contact->lport)  &&
        pdu.content_type != CTT_ID_DSC)
    {
        ui_log(LOG_ERR, "Client '%d' omitted identification!", n);
        return -1;
    }

    // check mandatory headers received
    if (contact->name[0] != '\0' && strcmp(contact->name, pdu.nickname) != 0)
    {
        ui_log(LOG_INFO, "'%s' changed nickname to '%s'!", contact->name,
               pdu.nickname);
    }

    if (contact->onion_id[0] != '\0' &&
        strcmp(contact->onion_id, pdu.onion_id) != 0)
    {
        ui_log(LOG_ERR, "'%s' changed Onion-ID! Contact will be removed!",
               contact->name);
        return -1;
    }

    if (contact->lport != 0 && contact->lport != pdu.lport)
    {
        ui_log(LOG_ERR, "'%s' changed Listening Port! Contact will be removed!",
               contact->name);
        return -1;
    }

    // set nickname of contact
    contact->name[0] = '\0';

    if (pdu.nickname[0] != '\0')
    {
        strncat(contact->name, pdu.nickname, MAX_NICKNAME);
    }

    // set onion id of contact
    contact->onion_id[0] = '\0';

    if (pdu.onion_id[0] != '\0')
    {
        strncat(contact->onion_id, pdu.onion_id, ONION_ADDRLEN);
    }

    // set listening port of contact
    contact->lport = pdu.lport;

    /*
     * == TEXT/PLAIN ==
     */
    if (pdu.content_type == CTT_ID_TXT)
    {
        // allocate memory for text message
        if ((txt_msg = malloc(pdu.content_length + 1)) == NULL)
        {
            ui_fatal("Memory allocation for text message failed!");
        }

        // store bytes from pdu in txt_msg and terminate it
        memcpy(txt_msg, pdu.content, pdu.content_length);
        txt_msg[pdu.content_length] = '\0';
        // print text message
        ui_write(pdu.nickname, txt_msg);
        free(txt_msg);
    }
    /*
     * == CONTROL/DISCOVER ==
     */
    else if (pdu.content_type == CTT_ID_DSC)
    {
        // since dchat brings with the problem of duplicate contacts
        // check if there are duplicate contacts in the contactlist
        if ((ret = check_duplicates(n)) != -1) //error
        {
            ui_log(LOG_INFO, "Detected duplicate contact - removing it!");
            del_contact(ret);  // delete duplicate
        }

        // iterate through the content of the pdu containing
        // the new contacts
        if ((ret = receive_contacts(&pdu)) == -1)
        {
            ui_log(LOG_WARN, "Could not add all contacts from the received contactlist!");
        }
    }
    /*
     * == UNKNOWN CONTENT-TYPE ==
     */
    else
    {
        ui_log(LOG_WARN, "Unknown Content-Type!");
    }

    free_pdu(&pdu);
    return len;
}


/**
 * Handles local connection requests from the user.
 * Connects to the remote client with the given onion address, who will
 * be added as contact. This new contact will be sent that all of our known
 * contacts as specified in the DChat protocol.
 * @param onion_id Destination onion address to connect to
 * @param port     Destination port to connect to
 * @return The index where the contact has been added in the contactlist,
 * -1 on error
 */
int
handle_local_conn_request(char* onion_id, uint16_t port)
{
    int s; // socket of the contact we have connected to
    int n; // index of the contact in our contactlist

    // connect to given address
    if ((s = create_tor_socket(onion_id, port)) == -1)
    {
        return -1;
    }
    else
    {
        // add contact
        if ((n = add_contact(s)) == -1)
        {
            ui_log_errno(LOG_ERR, "Could not add new contact!");
            return -1;
        }
        else
        {
            // set onion id of new contact
            _cnf->cl.contact[n].onion_id[0] = '\0';
            strncat(_cnf->cl.contact[n].onion_id, onion_id, ONION_ADDRLEN);
            // set listening port of new contact
            _cnf->cl.contact[n].lport = port;
            // send all our known contacts to the newly connected client
            send_contacts(n);
        }
    }

    return n;
}


/**
 * Handles connection requests from a remote client.
 * Accepts a connection from a remote client and so that a new chat session
 * will be established between this and the remote host. Moreover the remote
 * host will be added as new contact in the contactlist and the local contactlist
 * will be sent to him.
 * @see add_contact()
 * @return return value of function add_contact, or -1 on error
 */
int
handle_remote_conn_request()
{
    int s;                      // socket file descriptor
    int n;                      // index of new contact

    // accept connection request
    if ((s = accept(_cnf->acpt_fd, NULL, NULL)) == -1)
    {
        ui_log_errno(LOG_ERR, "Could not accept connection from remote host!");
        return -1;
    }

    // add new contact to contactlist
    if ((n = add_contact(s)) != -1)
    {
        ui_log(LOG_INFO, "Remote host (%d) connected!", n);
    }
    else
    {
        ui_log_errno(LOG_ERR, "Could not add new contact!");
        return -1;
    }

    _cnf->cl.contact[n].accepted = 1;
    send_contacts(n);
    return n;
}


/**
 * Cleanup ressources used by the thread `conn_th` holded by the
 * global config.
 * Closes the reading pipe `connect_fd` and the writing pipe end
 * of `cl_change` stored in the global config.
 */
void
cleanup_th_new_conn(void* arg)
{
    /// close pipes used for in `th_new_conn`
    close(_cnf->connect_fd[0]);
    close(_cnf->cl_change[1]);
}


/**
 * Thread function that reads an onion address from a pipe and connects to this
 * address.
 * Establishes new connections to the given addresses read from the global config
 * pipe `connect_fd`. It reads an onion address and then connects to
 * this address via TOR. If a connection has been established successfully, a new
 * contact will be added and the contactlist will be sent to him. Furthermore
 * the character '1' will be written to the global config pipe `cl_change`.
 * @see handle_local_conn_request()
 */
void*
th_new_conn()
{
    char onion_id[ONION_ADDRLEN + 1]; // onion address of remote host
    uint16_t port;                    // port of remote host
    char c = '1';                     // signal that a new connection has been established
    int ret;
    // setup cleanup handler and cancelation attributes
    pthread_cleanup_push(cleanup_th_new_conn, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    for (;;)
    {
        // read from pipe to get new address
        if ((ret = read(_cnf->connect_fd[0], onion_id, ONION_ADDRLEN)) == -1)
        {
            ui_log(LOG_WARN, "Could not read Onion-ID from connection pipe!");
        }

        // EOF
        if (!ret)
        {
            break;
        }

        // read from pipe to get new port
        if ((ret = read(_cnf->connect_fd[0], &port, sizeof(uint16_t))) == -1)
        {
            ui_log(LOG_WARN, "Could not read Listening-Port from connection pipe!");
        }

        // EOF
        if (!ret)
        {
            break;
        }

        // terminate address
        onion_id[ONION_ADDRLEN] = '\0';
        // lock contactlist
        pthread_mutex_lock(&_cnf->cl.cl_mx);

        if (handle_local_conn_request(onion_id, port) == -1)
        {
            ui_log(LOG_WARN, "Connection to remote host failed!");
        }
        else if ((write(_cnf->cl_change[1], &c, sizeof(c))) == -1)
        {
            ui_log(LOG_WARN, "Could not write to change pipe!");
        }

        // unlock contactlist
        pthread_mutex_unlock(&_cnf->cl.cl_mx);
    }

    // execute cleanup handler
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}


/**
 * Thread function that reads from stdin until the user hits enter.
 * Waits for new user input. If the user has entered something,
 * it will be written to the global config pipe `user_input`.
 * First the length (int) of the string will be written to the pipe and then each
 * byte of the string. On EOF of stdin 0 will be written
 * @return 0 on success, -1 in case of error
 */
int
th_new_input()
{
    char* line;                    // line read from stdin
    int len;                       // length of line

    while (1)
    {
        // read one line
        if (ui_read_line(&line) <= 0)
        {
            continue;
        }

        // remove newline character
        if ((len = is_valid_termination(line)) < 0)
        {
            //TODO error
            break;
        }

        line[len] = '\0';

        // EOF or user has entered "/exit"
        if (line == NULL || !strcmp(line, "/exit"))
        {
            break;
        }
        else
        {
            // user did not write anything -> just hit enter
            if (len == 0)
            {
                len = 1;

                if (write(_cnf->user_input[1], &len, sizeof(int)) == -1)
                {
                    return -1;
                }

                if (write(_cnf->user_input[1], "\n", len) == -1)
                {
                    return -1;
                }
            }
            else
            {
                // first write length of string
                if (write(_cnf->user_input[1], &len, sizeof(int)) == -1)
                {
                    return -1;
                }

                // then write the bytes of the string
                if (write(_cnf->user_input[1], line, len) == -1)
                {
                    return -1;
                }
            }

            free(line);
        }
    }

    return 0;
}


/**
 * Cleanup ressources used by the thread `select_th` holded by the
 * global config.
 * Closes the listening port, every contact file descriptor and
 * the reading pipe end of `user_input` and `cl_change`.
 */
void
cleanup_th_main_loop(void* arg)
{
    int i;
    // close local listening socket
    close(_cnf->acpt_fd);

    // close file descriptors of contacts
    for (i = 0; i < _cnf->cl.cl_size; i++)
    {
        if (_cnf->cl.contact[i].fd)
        {
            close(_cnf->cl.contact[i].fd);
        }
    }

    //close pipe stdin
    close(_cnf->user_input[0]);
    // close write pipe for main thread function th_main_loop
    close(_cnf->cl_change[0]);
}


/**
 * Main chat loop of this client.
 * This function is the main loop of DChat that selects(2) certain file
 * descriptors stored in the global configuration to read from.
 * It waits for local userinput, PDUs from remote clients, local connection
 * requests and remote connection requests. If select returns and a  file
 * descriptor can be read, this function will take action depending on which file
 * descriptor is able to read from.
 */
void*
th_main_loop()
{
    fd_set rset;    // list of readable file descriptors
    int nfds;       // number of fd in rset
    int ret;        // return value
    char c;         // for pipe: th_new_conn
    char* line;     // line returned from user input
    int cancel = 0; // cancel main loop
    int i;
    // setup cleanup handler and cancelation attributes
    pthread_cleanup_push(cleanup_th_main_loop, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    for (;;)
    {
        // INIT fd_set
        FD_ZERO(&rset);
        // ADD STDIN: pipe file descriptor that connects the thread
        // function 'th_new_input'
        FD_SET(_cnf->user_input[0], &rset);
        // ADD LISTENING PORT: acpt_fd of global config
        FD_SET(_cnf->acpt_fd, &rset);
        nfds = max(_cnf->user_input[0], _cnf->acpt_fd);
        // ADD NEW CONN: pipe file descriptor that connects the thead
        // function 'th_new_conn'
        FD_SET(_cnf->cl_change[0], &rset);
        nfds = max(nfds, _cnf->cl_change[0]);
        // ADD CONTACTS: add all contact socket file descriptors from
        // the contactlist
        pthread_mutex_lock(&_cnf->cl.cl_mx);

        for (i = 0; i < _cnf->cl.cl_size; i++)
        {
            if (_cnf->cl.contact[i].fd)
            {
                FD_SET(_cnf->cl.contact[i].fd, &rset);
                // determine fd with highest value
                nfds = max(_cnf->cl.contact[i].fd, nfds);
            }
        }

        pthread_mutex_unlock(&_cnf->cl.cl_mx);
        // used as backup since nfds will be overwritten if an
        // interrupt
        // occurs
        int old_nfds = nfds;
        pthread_testcancel();

        while ((nfds = select(old_nfds + 1, &rset, NULL, NULL, NULL))  <= 0)
        {
            pthread_testcancel();

            if (nfds == -1)
            {
                // something interrupted select(2) - try select again
                if (errno == EINTR)
                {
                    continue;
                }

                ui_log_errno(LOG_ERR, "select() failed!");
                cancel = 1;
                break;
            }
        }

        if (cancel)
        {
            break;
        }

        // CHECK STDIN: check if thread has written to the user_input
        // pipe
        if (FD_ISSET(_cnf->user_input[0], &rset))
        {
            nfds--;

            // read length of string from pipe
            if (read(_cnf->user_input[0], &ret, sizeof(int)) < 0)
            {
                break;
            }

            // allocate memory for the string entered from user
            line = malloc(ret + 1);

            // read string
            if (read(_cnf->user_input[0], line, ret) < 0 || ret <= 0)
            {
                free(line);
                break;
            }

            line[ret] = '\0';
            pthread_mutex_lock(&_cnf->cl.cl_mx);

            // handle user input
            if ((ret = handle_local_input(line)) == -1)
            {
                break;
            }

            pthread_mutex_unlock(&_cnf->cl.cl_mx);
            free(line);
        }

        // CHECK LISTENING PORT: check if new connection can be
        // accepted
        if (FD_ISSET(_cnf->acpt_fd, &rset))
        {
            nfds--;
            pthread_mutex_lock(&_cnf->cl.cl_mx);

            // handle new connection request
            if ((ret = handle_remote_conn_request()) == -1)
            {
                break;
            }

            pthread_mutex_unlock(&_cnf->cl.cl_mx);
        }

        // CHECK NEW CONN: check if user new connection has been added
        if (FD_ISSET(_cnf->cl_change[0], &rset))
        {
            nfds--;

            // !< EOF
            if (read(_cnf->cl_change[0], &c, sizeof(c)) < 0)
            {
                break;
            }
        }

        // CHECK CONTACTS: check file descriptors of contacts
        // check if nfds is 0 => no contacts have written something
        pthread_mutex_lock(&_cnf->cl.cl_mx);

        for (i = 0; nfds && i < _cnf->cl.cl_size; i++)
        {
            if (FD_ISSET(_cnf->cl.contact[i].fd, &rset))
            {
                nfds--;

                // handle input from remote user
                // -1 = error, 0 = EOF
                if ((ret = handle_remote_input(i)) == -1 || ret == 0)
                {
                    del_contact(i);
                }
            }
        }

        pthread_mutex_unlock(&_cnf->cl.cl_mx);
    }

    //execute cleanup handler
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}
