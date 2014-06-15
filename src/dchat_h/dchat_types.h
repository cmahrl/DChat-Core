#ifndef DCHAT_TYPES_H
#define DCHAT_TYPES_H

#include <netinet/in.h>


#define FRAME_BUF_LEN  4096
#define INIT_CONTACTS  30
#define MAX_NICKNAME   31


//*********************************
//          TYPEDEFS
//*********************************

/*!
 * Structure for a DChat PDU
 */
typedef struct dchat_pdu
{
    int content_type;   //!< type of message
    char* content;      //!< content part of message
    int content_length; //!< lengh of content
    int listen_port;    //!< listening port of the client
} dchat_pdu_t;

/*!
 * Structure for contact information
 */
typedef struct contact
{
    int fd;     //!< file descriptor of TCP session
    int lport;  //!< listening port

    union       //!< ip address
    {
        struct sockaddr_in v4addr;
        struct sockaddr_in6 v6addr;
        struct sockaddr_storage stor;
    };
    char buf[FRAME_BUF_LEN];    //!< framing buffer
    int len;                    //!< number of bytes in buffer
    int chatrooms;              //!< member of this chat rooms
    char name[32];              //!< nickname
} contact_t;

/*!
 * Structure storing client contacts
 */
typedef struct contactlist
{
    contact_t* contact;         //!< array of contacts
    pthread_mutex_t cl_mx;      //!< mutex to signal lock
    int cl_size;                //!< size of array
    int used_contacts;          //!< elements used in contact array
} contactlist_t;

/*!
 * Structure for global configurations
 */
typedef struct dchat_conf
{
    contactlist_t cl;           //!< contact list
    contact_t me;               //!< local contact information
    int acpt_fd;                //!< listening socket
    int in_fd, out_fd;          //!< console input and output
    int connect_fd[2];          //!< pipe to connector
    int cl_change[2];           //!< pipe to signal wait loop from connect
    int user_input[2];          //!< pipe to signal a new user input from stdin
    pthread_t conn_th;          //!< thread responsible for new connections
    pthread_t userin_th;        //!< thread responsible for user input
} dchat_conf_t;

#endif
