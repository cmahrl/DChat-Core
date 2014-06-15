/*
 * =====================================================================================
 *
 *       Filename:  dchat.c
 *
 *    Description:  DCHAT is a peer-to-peer chat program providing filesharing within
 *                  a chat. This file contains several core functions of DCHAT like
 *                      -) Handler for accepting connections
 *                      -) Handler for user input
 *                      -) Hanlder for remote input
 *                      -) Handler for filesharing
 *                      -) etc.
 *
 *        Version:  1.0
 *        Created:  22/05/2014
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Authors: Christoph Mahrl (clm), christoph.mahrl@gmail.com
 *   Organization:  University of Applied Sciences St. Poelten - IT-Security
 *
 * =====================================================================================
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

#include "dchat_h/dchat.h"
#include "dchat_h/dchat_types.h"
#include "dchat_h/dchat_decoder.h"
#include "dchat_h/dchat_contact.h"
#include "dchat_h/dchat_cmd.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"



// *********************************
// GLOBAL
// *********************************
dchat_conf_t cnf;       // Global dchat configuration structure
int ret_exit_ = EXIT_SUCCESS;


// *********************************
// MAIN
// *********************************


int
main(int argc, char** argv)
{
    int acpt_port;        // !< local listening port
    int remote_port;      // !< port of remote client
    char* interface = NULL;   // !< network-int to bind to and
    // listen for connections
    char* remote_address = NULL;  // !< address of remote
    // client to connect to
    char* app = NULL;     // !< Name of application -> DChat
    char* nickname = NULL;    // !< Nickname of local client
    struct sockaddr_in da;    // !< structur where remote address will
    // be stored
    // !< strip path of this binary name and use the result as app name
    app = basename(argv[0]);

    if(app == NULL)
    {
        log_msg(LOG_ERR, "basename() failed '%s'", strerror(errno));
        return EXIT_FAILURE;
    }

    // !< see usage
    if(argc < 4 || argc == 5 || argc > 6)
    {
        usage(app);
        return EXIT_FAILURE;
    }

    else
    {
        interface = argv[1];
        acpt_port = atoi(argv[2]);
        nickname = argv[3];
    }

    // !< init dchat (e.g. global configuration, listening socket, ...)
    if(init(&cnf, interface, acpt_port, nickname) == -1)
    {
        return EXIT_FAILURE;
    }

    // !< has a remoteaddress and a remoteport been specified?
    if(argc == 6)
    {
        remote_address = argv[4];
        remote_port = atoi(argv[5]);
        // !< init address structure
        memset(&da, 0, sizeof(da));
        da.sin_family = AF_INET;
        da.sin_port = htons(remote_port);   // port for the connection

        // !< init dst ip for address structure
        if(inet_pton(AF_INET, remote_address, &da.sin_addr) != 1)
        {
            log_errno(LOG_ERR, "wrong format of ip address");
        }

        // !< connect to new contact, add him as contact, and send
        // contactlist to him
        if(handle_local_conn_request(&cnf, (struct sockaddr_storage*) &da) ==
                -1)
        {
            log_errno(LOG_WARN,
                      "main(): Could not execute connection request successfully");
        }
    }

    /*
     * ! main loop: waits for filedescriptors to read data from and act
     * correspondently
     */
    ret_exit_ = th_main_loop(&cnf);
    // !< terminate this process
    raise(SIGTERM);
    return ret_exit_ ? EXIT_FAILURE : EXIT_SUCCESS;
}


// *********************************
// PROTO.IMPL
// *********************************


/*
 * !Initializes the pipes and threads used for the this program.
 * Furtermore the global configuration gets initialized with its listening
 * socket, nickname and other basic things. @param cnf: the global config
 * structure @param interface: interface where this executable will be
 * bound to and listening for sockets @param acpt_port: the port for local
 * socket, where remote clients can connect to @param nickname: nickname
 * used for this chat session @return 0 on successful initialization, -1
 * in case of an error
 */
int
init(dchat_conf_t* cnf, char* interface, int acpt_port, char* nickname)
{
    struct sigaction sa_terminate;
    sa_terminate.sa_handler = terminate;
    sigemptyset(&sa_terminate.sa_mask);
    sa_terminate.sa_flags = 0;
    sigaction(SIGHUP, &sa_terminate, NULL);   // !< terminal line hangup
    sigaction(SIGQUIT, &sa_terminate, NULL);  // !< quit programm
    sigaction(SIGINT, &sa_terminate, NULL);   // !< interrupt programm
    sigaction(SIGTERM, &sa_terminate, NULL);  // !< software termination

    // !< check if given port is valid
    if(acpt_port < 1 || acpt_port > 65535)
    {
        log_msg(LOG_WARN, "Invalid port for listening socket");
        return -1;
    }

    // !< init the listening socket
    if(init_global_config(cnf, interface, acpt_port, nickname) == -1)
    {
        return -1;
    }

    // !< pipe to the th_new_conn
    if(pipe(cnf->connect_fd) == -1)
    {
        log_errno(LOG_ERR, "could not create pipe");
        return -1;
    }

    // !< pipe to send signal to wait loop from connect
    if(pipe(cnf->cl_change) == -1)
    {
        log_errno(LOG_ERR, "could not create cl_change pipe");
        return -1;
    }

    // !< pipe to signal new user input from stdin
    if(pipe(cnf->user_input) == -1)
    {
        log_errno(LOG_ERR, "could not create cl_change pipe");
        return -1;
    }

    // !< init the mutex function used for locking the contactlist
    if(pthread_mutex_init(&cnf->cl.cl_mx, NULL))
    {
        log_errno(LOG_ERR, "pthread_mutex_init() failed");
        return -1;
    }

    // !< create new th_new_conn-thread
    if(pthread_create
            (&cnf->conn_th, NULL, (void* (*)(void*)) th_new_conn, cnf) == -1)
    {
        log_errno(LOG_ERR, "pthread_create() failed");
        return -1;
    }

    // !< create new thread for handling userinput from stdin
    if(pthread_create
            (&cnf->userin_th, NULL, (void* (*)(void*)) th_new_input, cnf) == -1)
    {
        log_errno(LOG_ERR, "pthread_create() failed");
        return -1;
    }

    return 0;
}


/*
 * ! Initialize the global configuration: bind to an interface, create a
 * socket for listening, set nickname and set all initialization
 * configuration in the structure @param cnf: Pointer to global
 * configuration structure @param interface: Network interface to bind to
 * @param acpt_port: Port to bind to and listen for incoming connetion
 * requests @param nickname: Nickname used for this chat session @return
 * socket descriptor or -1 if an error occurs
 */
int
init_global_config(dchat_conf_t* cnf, char* interface, int acpt_port,
                   char* nickname)
{
    struct ifaddrs*
            ifaddr;   // !< contains information of all network interfaces of this computer
    struct ifaddrs* ifa;      // !< used to iterate through the network interfaces
    struct sockaddr sa;       // !< used for storing an ip address of a
    // network interface temporarily
    char addr_str[INET6_ADDRSTRLEN + 1];  // !< string
    // representation
    // of ip address
    int s;            // !< socket file descriptor where this
    // client will be listening
    int on = 1;           // !< used to turn a socket option on
    int found_int = 0;        // !< used to check if the given
    // interface was found

    // !< create socket
    if((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
{
        log_errno(LOG_ERR, "socket() failed");
        return -1;
    }

    // !< socketoption to prevent: 'bind() address already in use'
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        log_errno(LOG_ERR, "setsockopt() failed");
        close(s);
        return -1;
    }

    // !< get information about all network interfaces
    if(getifaddrs(&ifaddr) == -1)
    {
        log_errno(LOG_ERR, "getifaddrs() failed");
        close(s);
        return -1;
    }

    // !< iterate through network interfaces and set ip of interface
    memset(&sa, 0, sizeof(sa));

    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        // TODO: Support for IPv6
        if(ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET)
        {
            // !< is it the interface we are looking for?
            if(!strncmp(interface, ifa->ifa_name, strlen(ifa->ifa_name)))
            {
                if(ifa->ifa_addr != NULL)
                {
                    // !< temporarily store address
                    memcpy(&sa, ifa->ifa_addr, sizeof(struct sockaddr));
                    found_int = 1;    // !< we found the given interface
                    break;
                }

                else
                {
                    log_msg(LOG_ERR,
                            "Interface '%s' does not have an ip address bound to it",
                            ifa->ifa_name);
                    close(s);
                    freeifaddrs(ifaddr);
                    return -1;
                }
            }
        }
    }

    // !< interface has not been found
    if(!found_int)
    {
        log_msg(LOG_ERR, "Interface '%s' not found", interface);
        close(s);
        freeifaddrs(ifaddr);
        return -1;
    }

    freeifaddrs(ifaddr);

    // !< add port to socketaddress structure
    if(ip_version((struct sockaddr_storage*) &sa) == 4)
    {
        ((struct sockaddr_in*) &sa)->sin_port = htons(acpt_port);
    }

    else if(ip_version((struct sockaddr_storage*) &sa) == 6)
    {
        ((struct sockaddr_in6*) &sa)->sin6_port = htons(acpt_port);
    }

    // !< bind socket to a specified interface e.g. "eth0"
    if(bind(s, &sa, sizeof(struct sockaddr)) == -1)
    {
        log_errno(LOG_ERR, "bind() failed");
        close(s);
        return -1;
    }

    // !< listen on this socket
    if(listen(s, 5) == -1)
    {
        log_errno(LOG_ERR, "listen() failed");
        close(s);
        return -1;
    }

    // !< log, where on which address and port this client will be
    // listening
    if(ip_version((struct sockaddr_storage*) &sa) == 4)
    {
        log_msg(LOG_INFO, "Listening on '%s:%d'",
                inet_ntop(AF_INET, &((struct sockaddr_in*) &sa)->sin_addr,
                          addr_str, INET_ADDRSTRLEN), acpt_port);
    }

    else if(ip_version((struct sockaddr_storage*) &sa) == 6)
    {
        log_msg(LOG_INFO, "Listening on '%s:%d'",
                inet_ntop(AF_INET6, &((struct sockaddr_in6*) &sa)->sin6_addr,
                          addr_str, INET6_ADDRSTRLEN), acpt_port);
    }

    // !< init the global config structure
    memset(cnf, 0, sizeof(*cnf));
    cnf->in_fd = 0;       // !< use stdin as input source
    cnf->out_fd = 1;      // !< use stdout as output source
    cnf->cl.cl_size = 0;      // !< set initial size of contactlist
    cnf->cl.used_contacts = 0;    // !< no known contacts, at start
    cnf->me.name[0] = '\0';
    strncat(cnf->me.name, nickname, MAX_NICKNAME);    // !< set nickname
    // of this chat
    // session
    cnf->me.lport = acpt_port;    // !< set listening-port
    // !< set socket address, where this client is listening
    memcpy(&cnf->me.stor, &sa, sizeof(struct sockaddr_storage));
    cnf->acpt_fd = s;     // !< set socket file descriptor
    return s;         // !< return socket file descriptor
}


/*
 * ! Closes all open file descriptors, sockets, pipes and mutexes of dchat
 * @param cnf: Global config structure, storing contact file descriptors,
 * etc...
 */
void
destroy(dchat_conf_t* cnf)
{
    int i;

    // !< close file descriptors of contacts
    for(i = 0; i < cnf->cl.cl_size; i++)
    {
        if(cnf->cl.contact[i].fd)
        {
            close(cnf->cl.contact[i].fd);
        }
    }

    // !< close pipe to th_new_conn
    close(cnf->connect_fd[1]);
    // !< close pipe to th_main_loop
    close(cnf->cl_change[1]);
    // !< close pipe to signal new user input from stdin
    close(cnf->user_input[1]);
    // !< close socket
    close(cnf->acpt_fd);
    // !< destroy contact mutex
    pthread_mutex_destroy(&cnf->cl.cl_mx);
}


/*
 * ! Signal handler function, that will free all used resources e.g. file
 * descriptors, pipes, etc... and will terminate this process afterwards
 * @param sig: Type of signal (e.g. SIGTERM, ...)
 */
void
terminate(int sig)
{
    // !< free all resources used like file descriptors, sockets etc...
    destroy(&cnf);
    ansi_term_clear_line(cnf.out_fd);
    ansi_term_cr(cnf.out_fd);
    dprintf(cnf.out_fd, "Good Bye!\n");
    ret_exit_ == 0 ? exit(EXIT_SUCCESS) : exit(EXIT_FAILURE);
}


/*
 * ! Interpretes the given line and reacts correspondently to it. If the
 * line is a command it will be executed, otherwise it will be treated as
 * text message and send to all known contacts stored in the contactlist
 * in the global configuration.  @param cnf: Pointer to global
 * configuration @return -2 on warning, -1 on error, 0 on success, 1 on
 * exit
 */
int
handle_local_input(dchat_conf_t* cnf, char* line)
{
    dchat_pdu_t msg;
    int i, ret = 0, len, cmd;

    // !< user entered command
    if((cmd = parse_cmd(cnf, line)) == 0)
    {
        return 0;
    }

    // !< command has wrong syntax
    else if(cmd == -2)
    {
        log_msg(LOG_WARN, "Syntax error");
        return -2;
    }

    // !< no command has been entered
    else
    {
        len = strlen(cnf->me.name) + 2;     // !< memory for local nickname +2
        // for ": "
        len += strlen(line);    // !< memory for text message

        if(len != 0)
        {
            // !< init dchat pdu
            memset(&msg, 0, sizeof(msg));
            msg.content_type = CT_TXT_PLAIN;
            msg.listen_port = cnf->me.lport;

            // !< allocate memory the size of nickname + 2 + text message
            if((msg.content = malloc(len + 1)) == NULL)
            {
                log_errno(LOG_ERR,
                          "handle_local_input() failed, could not allocated memory");
                return -1;
            }

            // !< append the content
            msg.content[0] = '\0';
            strncat(msg.content, cnf->me.name, strlen(cnf->me.name));
            strncat(msg.content, ": ", 2);
            strncat(msg.content, line, strlen(line));
            msg.content_length = len; // !< length of content excluding
            // \0

            // !< write pdu to known contacts
            for(i = 0; i < cnf->cl.cl_size; i++)
            {
                if(cnf->cl.contact[i].fd)
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

    // !< return if write_pdu() succeded
    return ret != -1 ? 0 : -1;
}


/*
 * ! Reads from contact specified with n on the specific file descriptor.
 * @param cnf: Pointer to dchat_conf_t structure holding the contact list
 * @param n: Index of contact in the respective contactlist @return length
 * of bytes read, 0 on EOF or -1 on error
 */
int
handle_remote_input(dchat_conf_t* cnf, int n)
{
    dchat_pdu_t* pdu;     // !< pdu read from contact file
    // descriptor
    char* txt_msg;        // !< message used to store remote input
    int ret;          // !< return value
    int len;          // !< amount of bytes read
    int fd;           // !< file descriptor of a contact
    fd = cnf->cl.contact[n].fd;

    // !< read pdu from file descriptor (-1 indicates error)
    if((len = read_pdu(fd, &pdu)) == -1)
    {
        log_msg(LOG_ERR, "Illegal PDU from contact %d", n);
        return -1;
    }

    // !< EOF
    else if(!len)
    {
        log_msg(LOG_INFO, "contact '%d' disconnected", n);
        return 0;
    }

    // !< the first pdu of a newly connected client has to be a
    // "control/discover"
    // !< otherwise raise an error and delete this contact
    if(!cnf->cl.contact[n].lport && pdu->content_type != CT_CTRL_DISC)
    {
        log_msg(LOG_ERR, "Contact %d has not identfied himself", n);
        return -1;
    }

    else if(!cnf->cl.contact[n].lport && pdu->content_type == CT_CTRL_DISC)
    {
        // !< set listening port of contact
        cnf->cl.contact[n].lport = pdu->listen_port;
    }

    // !< CHECK CONTENT-TYPE OF PDU

    /*
     * !== TEXT/PLAIN ==
     */
    if(pdu->content_type == CT_TXT_PLAIN)
    {
        // !< allocate memory for text message
        if((txt_msg = malloc(pdu->content_length + 1)) == NULL)
        {
            log_errno(LOG_ERR,
                      "handle_remote_input() failed - Could not allocate memory");
            free_pdu(pdu);
            return -1;
        }

        // !< store bytes from pdu in txt_msg and terminate it
        memcpy(txt_msg, pdu->content, pdu->content_length);
        txt_msg[pdu->content_length] = '\0';
        // !< print text message
        print_dchat_msg(txt_msg, cnf->out_fd);
        free(txt_msg);
    }

    /*== CONTROL/DISCOVER == */
    else if(pdu->content_type == CT_CTRL_DISC)
    {
        // !< since dchat brings with the problem of duplicate contacts
        // !< check if there are duplicate contacts in the contactlist
        if((ret = check_duplicates(cnf, n)) == -2)
        {
            // !< ERROR
            log_msg(LOG_ERR, "check_duplicate_contact() failed");
        }

        else if(ret == -1)
        {
            // !< no duplicates
            // !< log_msg(LOG_INFO, "no duplicates founds");
        }
        else
        {
            // !< duplicates found
            log_msg(LOG_INFO, "Duplicate detected - Removing it (%d)", ret);
            del_contact(cnf, ret);    // !< delete contact from the
            // returned index
        }

        // !< iterate through the content of the pdu, parse contacts,
        // connect to them and add them
        // !< as new contacts
        if((ret = receive_contacts(cnf, pdu)) == -1)
        {
            log_msg(LOG_WARN,
                    "handle_remote_input() - 'control/discover' failed:  Not every new contact could be added");
        }

        else if(ret == 0)
        {
            // !< log_msg(LOG_INFO, "client %d does not know any other
            // contacts we do not know about", n);
        }
        else
        {
            // !< log_msg(LOG_INFO, "%d new client(s) have been added",
            // ret);
        }
    }

    /*
     * !== UNKNOWN CONTENT-TYPE ==
     */
    else
    {
        log_msg(LOG_WARN,
                "handle_remote_input(): Content-Type not implemented yet");
    }

    free_pdu(pdu);
    return len;           // !< amount of bytes read from file
    // descriptor
}


/*
 * ! Connects to the given socket address, adds it as contact and send
 * that contact all known contacts as specified in the DCHAT protocol
 * (see: Contact Exchange) @param cnf: Pointer to global configuration
 * holding the contactlist @param da: Destination address to connect to
 * @return The index where the contact has been added in the contactlist,
 * -1 on error
 */
int
handle_local_conn_request(dchat_conf_t* cnf, struct sockaddr_storage* da)
{
    int s;            // !< socket of the contact we connect to
    int n;            // !< index of the contact we will add to
    // our contactlist

    // !< connect to given address
    if((s = connect_to((struct sockaddr*) da)) == -1)
    {
        return -1;
    }

    else
    {
        // !< add contact
        if((n = add_contact(cnf, s, da)) == -1)
        {
            log_errno(LOG_ERR,
                      "handle_local_conn_request() failed - Could not add contact");
            return -1;
        }

        else
        {
            // !< set listening port of contact (we now it, since we
            // connected to it)
            if(ip_version(da) == 4)
            {
                cnf->cl.contact[n].lport =
                    ntohs(((struct sockaddr_in*) da)->sin_port);
            }

            else if(ip_version(da) == 6)
            {
                cnf->cl.contact[n].lport =
                    ntohs(((struct sockaddr_in6*) da)->sin6_port);
            }

            // !< send contacts to newly connected client
            // !< DCHAT protocol defines, that after a "connect" or
            // "accept" the contactlist has to be sent
            send_contacts(cnf, n);
        }
    }

    return n;
}


/*
 * ! Accepts a TCP syn request from a client and so that a new TCP
 * connection will be established.  Moreover a new contact (with the
 * specific socket settings) will be added to the contactlist @param cnf:
 * Pointer to global configuration @return return code of function
 * add_contact, or -1 on error
 */
int
handle_remote_conn_request(dchat_conf_t* cnf)
{
    int s;            // !< socket file descriptor
    int n;            // !< index of new contact
    struct sockaddr_storage ss;   // !< socketaddress of new contact
    socklen_t socklen;        // !< length of socketaddress
    socklen = sizeof(ss);

    // !< accept connection request
    if((s = accept(cnf->acpt_fd, (struct sockaddr*) &ss, &socklen)) == -1)
    {
        log_errno(LOG_ERR, "accept() failed");
        return -1;
    }

    // !< add new contact to contactlist
    n = add_contact(cnf, s, &ss);

    if(n != -1)
    {
        log_msg(LOG_INFO, "contact %d connected", n);
    }

    return n;
}


/*
 * ! Establishes new connections to the given addresses read from the pipe
 * which pointer is stored in the global configuration. This function
 * reads a sockaddr_storage structure and then tries to connect to it
 * (see: handle_local_conn_request) @param cnf: Pointer to global config
 * structure to read from the pipe connect_fd
 */
void*
th_new_conn(dchat_conf_t* cnf)
{
    struct sockaddr_storage da;
    char c = '1';
    int ret;

    for(;;)
    {
        // !< read from pipe to get new addresses
        if((ret = read(cnf->connect_fd[0], &da, sizeof(da))) == -1)
        {
            log_msg(LOG_WARN, "th_new_conn(): could not read from pipe");
        }

        // !< EOF
        if(!ret)
        {
            close(cnf->connect_fd[0]);
            close(cnf->cl_change[1]);
            break;
        }

        // !< lock contactlist
        pthread_mutex_lock(&cnf->cl.cl_mx);

        if(handle_local_conn_request(cnf, &da) == -1)
        {
            log_errno(LOG_WARN,
                      "th_new_conn(): Could not execute connection request successfully");
        }

        else if((write(cnf->cl_change[1], &c, sizeof(c))) == -1)
        {
            log_msg(LOG_WARN, "th_new_conn(): could not write to pipe");
        }

        // !< unlock contactlist
        pthread_mutex_unlock(&cnf->cl.cl_mx);
    }

    pthread_exit(NULL);
}


/*
 * ! Waits for new user input on stdin using GNU readline. If the user has
 * entered something, it will be written to the pipe user_input stored in
 * the global configuration. First the length (int) of the string will be
 * written to the pipe.  Then each byte of the string. On EOF 0 will be
 * written or \n if String is empty.  @param cnf: Pointer to global config
 * structure to write to the pipe user_input
 */
void*
th_new_input(dchat_conf_t* cnf)
{
    char prompt[MAX_NICKNAME + 8];
    char* line;
    int len;
    // !< Assemble prompt for GNU readline
    prompt[0] = '\0';
    strncat(prompt, "Me (", 4);
    strncat(prompt, cnf->me.name, MAX_NICKNAME);
    strncat(prompt, "): ", 3);

    while(1)
    {
        /*
         * ! wait until user entered a line that can be read from stdin
         */
        line = readline(prompt);

        // !< EOF or user has entered "/exit"
        if(line == NULL || !strcmp(line, "/exit"))
        {
            // !< since EOF is not a signal - raise SIGTERM
            // !< to terminate this process
            raise(SIGTERM);
            break;
        }

        else
        {
            len = strlen(line);

            // !< user did not write anything -> just hit enter
            if(len == 0)
            {
                len = 1;
                write(cnf->user_input[1], &len, sizeof(int));
                write(cnf->user_input[1], "\n", len);
            }

            else
            {
                // !< first write length of string
                write(cnf->user_input[1], &len, sizeof(int));
                // !< then write the bytes of the string
                write(cnf->user_input[1], line, len);
            }

            free(line);
        }
    }

    pthread_exit(NULL);
}


/*
 * ! This function is the main loop of DChat that selects(2) certain file
 * descriptors stored in the global configuration structure to read from.
 * E.g. it waits for userinput, accepts, new connections and so on. After
 * a file descriptor can be read, it takes action depending on which file
 * descriptor is able to read from.  @param cnf Pointer to global config
 * structure holding all file descriptors (e.g.  accept file descriptor,
 * input file descriptors, pipes, ...) @return 0 if no error, -1 if error
 * occurs
 */
int
th_main_loop(dchat_conf_t* cnf)
{
    fd_set rset;          // !< list of readable file descriptors
    int nfds;         // !< number of fd in rset
    int i;
    int ret;          // !< return value
    char c;           // !< for pipe: th_new_conn
    char* line;           // !< line returned from GNU readline

    for(;;)
    {
        // !< INIT fd_set
        FD_ZERO(&rset);
        // !< ADD STDIN: pipe file descriptor that connects the thread
        // function 'th_new_input'
        FD_SET(cnf->user_input[0], &rset);
        // !< ADD LISTENING PORT: acpt_fd of global config
        FD_SET(cnf->acpt_fd, &rset);
        nfds = max(cnf->user_input[0], cnf->acpt_fd);
        // !< ADD NEW CONN: pipe file descriptor that connects the thead
        // function 'th_new_conn'
        FD_SET(cnf->cl_change[0], &rset);
        nfds = max(nfds, cnf->cl_change[0]);
        // !< ADD CONTACTS: add all contact socket file descriptors from
        // the contactlist
        pthread_mutex_lock(&cnf->cl.cl_mx);

        for(i = 0; i < cnf->cl.cl_size; i++)
        {
            if(cnf->cl.contact[i].fd)
            {
                FD_SET(cnf->cl.contact[i].fd, &rset);
                nfds = max(cnf->cl.contact[i].fd, nfds);    // !<
                // determine
                // fd with
                // highest
                // value
            }
        }

        pthread_mutex_unlock(&cnf->cl.cl_mx);
        // !< used as backup since nfds will be overwritten if an
        // interrupt
        // !< occurs
        int old_nfds = nfds;

        while((nfds = select(old_nfds + 1, &rset, NULL, NULL, NULL)) == -1)
        {
            // !< something interrupted select(2) - try select again
            if(errno == EINTR)
            {
                continue;
            }

            else
            {
                log_errno(LOG_ERR, "select() failed");
                return -1;
            }
        }

        // !< CHECK STDIN: check if thread has written to the user_input
        // pipe
        if(FD_ISSET(cnf->user_input[0], &rset))
        {
            nfds--;
            // !< read length of string from pipe
            read(cnf->user_input[0], &ret, sizeof(int));

            // !< EOF
            if(!ret)
            {
                close(cnf->user_input[0]);
                return 0;
            }

            // !< allocate memory for the string entered from user
            line = malloc(ret + 1);
            // !< read string
            read(cnf->user_input[0], line, ret);
            line[ret] = '\0';
            pthread_mutex_lock(&cnf->cl.cl_mx);

            // !< handle user input
            if((ret = handle_local_input(cnf, line)) == -1)
            {
                return -1;
            }

            pthread_mutex_unlock(&cnf->cl.cl_mx);
            free(line);
        }

        // !< CHECK LISTENING PORT: check if new connection can be
        // accepted
        if(FD_ISSET(cnf->acpt_fd, &rset))
        {
            nfds--;
            pthread_mutex_lock(&cnf->cl.cl_mx);

            // !< handle new connection request
            if((ret = handle_remote_conn_request(cnf)) == -1)
            {
                return -1;
            }

            else
            {
                send_contacts(cnf, ret);
            }

            pthread_mutex_unlock(&cnf->cl.cl_mx);
        }

        // !< CHECK NEW CONN: check if user new connection has been added
        if(FD_ISSET(cnf->cl_change[0], &rset))
        {
            nfds--;

            // !< EOF
            if(!read(cnf->cl_change[0], &c, sizeof(c)))
            {
                close(cnf->cl_change[0]);
                return 0;
            }
        }

        // !< CHECK CONTACTS: check file descriptors of contacts
        // !< check if nfds is 0 => no contacts have written something
        pthread_mutex_lock(&cnf->cl.cl_mx);

        for(i = 0; nfds && i < cnf->cl.cl_size; i++)
        {
            if(FD_ISSET(cnf->cl.contact[i].fd, &rset))
            {
                nfds--;

                // !< handle input from remote user
                // !< -1 = error, 0 = EOF
                if((ret = handle_remote_input(cnf, i)) == -1 || ret == 0)
                {
                    close(cnf->cl.contact[i].fd);
                    del_contact(cnf, i);
                }
            }
        }

        pthread_mutex_unlock(&cnf->cl.cl_mx);
    }
}


/*
 * ! Prints the usage of this client program.  @param app: Pointer to a
 * string which holds the name of the program
 */
void
usage(char* app)
{
    fprintf(stderr, "Usage: %s <INT> <LPORT> <NICK> [<DST> <RPORT>]\n", app);
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr,
            "  INT   ...  mandatory; Interface where a tcp socket will be bound to and listening (e.g. en0, ...)\n");
    fprintf(stderr,
            "  LPORT ...  mandatory; Port where %s will be listening for incoming TCP SYN requests\n",
            app);
    fprintf(stderr,
            "  NICK  ...  mandatory; Defines the nickname that will be used for a chat session\n");
    fprintf(stderr,
            "  DST   ...  optional; Remote IP address of a client to connect with and start a chat\n");
    fprintf(stderr,
            "  RPORT ...  optional(*); Remote TCP-Port of a client waiting for connection requests on this port\n");
    fprintf(stderr, "\n  (*) Option is required if DST is specified!\n");
}
