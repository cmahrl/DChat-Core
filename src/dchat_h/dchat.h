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
#include "dchat_types.h"


#define DEFAULT_PORT 7777


//*********************************
//      INIT/DESTROY FUNCTIONS
//*********************************
int init(dchat_conf_t* cnf, struct sockaddr_storage* sa, int acpt_port, char* nickname);
int init_global_config(dchat_conf_t* cnf, struct sockaddr_storage* sa, int acpt_port,
                       char* nickname);
void destroy(dchat_conf_t* cnf);


//*********************************
//      HANDLER FUNCTIONS
//*********************************
void terminate(int sig);
int handle_local_input(dchat_conf_t* cnf, char* line);
int handle_remote_input(dchat_conf_t* cnf, int n);
int handle_local_conn_request(dchat_conf_t* cnf, struct sockaddr_storage* da);
int handle_remote_conn_request(dchat_conf_t* cnf);


//*********************************
//      THREAD FUNCTIONS
//*********************************
void* th_new_conn(dchat_conf_t* cnf);
void* th_new_input(dchat_conf_t* cnf);
int th_main_loop(dchat_conf_t* cnf);


//*********************************
//      MISC FUNCTIONS
//*********************************
void usage();

#endif
