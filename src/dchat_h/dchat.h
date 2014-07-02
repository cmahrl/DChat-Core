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


#ifndef DCHAT_H
#define DCHAT_H

#include <sys/socket.h>
#include <stdint.h>

#include "types.h"


//*********************************
//      COMMAND LINE OPTIONS
//*********************************
#define CLI_OPT_LONION 's'
#define CLI_OPT_NICK   'n'
#define CLI_OPT_LPORT  'l'
#define CLI_OPT_RONION 'd'
#define CLI_OPT_RPORT  'r'
#define CLI_OPT_HELP   'h'


//*********************************
//      GENERAL SETTINGS
//*********************************
#define DEFAULT_PORT   7777
#define LISTEN_ADDR    "127.0.0.1"
#define LISTEN_BACKLOG 20


//*********************************
//      INIT/DESTROY FUNCTIONS
//*********************************
int init(dchat_conf_t* cnf, struct sockaddr_storage* sa, char* onion_id,
         char* nickname);
int init_global_config(dchat_conf_t* cnf, struct sockaddr_storage* sa,
                       char* onion_id, char* nickname);
void destroy(dchat_conf_t* cnf);
void cleanup_th_new_conn(void* arg);
void cleanup_th_main_loop(void* arg);


//*********************************
//      HANDLER FUNCTIONS
//*********************************
void terminate(int sig);
int handle_local_input(dchat_conf_t* cnf, char* line);
int handle_remote_input(dchat_conf_t* cnf, int n);
int handle_local_conn_request(dchat_conf_t* cnf, char* onion_id, uint16_t port);
int handle_remote_conn_request(dchat_conf_t* cnf);


//*********************************
//      THREAD FUNCTIONS
//*********************************
void* th_new_conn(dchat_conf_t* cnf);
int th_new_input(dchat_conf_t* cnf);
void*  th_main_loop(dchat_conf_t* cnf);

#endif
