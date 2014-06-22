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
    int content_type;                //!< type of message
    char* content;                   //!< content part of message
    int content_length;              //!< lengh of content
    int listen_port;                 //!< listening port of the client
    char* nickname;                  //!< nickname of the client
} dchat_pdu_t;

/*!
 * Structure for contact information
 */
typedef struct contact
{
    int fd;     //!< file descriptor of TCP session
    int lport;  //!< listening port
    /**
     * Socket address of contact
     */
    union
    {
        struct sockaddr_in v4addr;
        struct sockaddr_in6 v6addr;
        struct sockaddr_storage stor;
    };
    char buf[FRAME_BUF_LEN];     //!< framing buffer
    int len;                     //!< number of bytes in buffer
    int chatrooms;               //!< member of this chat rooms
    char name[MAX_NICKNAME + 1]; //!< nickname
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
    pthread_t select_th;        //!< thread responsible for select(2) fd
} dchat_conf_t;

#endif
