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
//      GENERAL SETTINGS
//*********************************
#define DEFAULT_PORT   7777
#define LISTEN_ADDR    "127.0.0.1"
#define LISTEN_BACKLOG 20


//*********************************
//      INIT/DESTROY FUNCTIONS
//*********************************
int init_global_config();
int init_listening(char* address);
int init_threads();
void destroy();
void cleanup_th_new_conn(void* arg);
void cleanup_th_main_loop(void* arg);


//*********************************
//      HANDLER FUNCTIONS
//*********************************
void terminate(int sig);
int handle_local_input(char* line);
int handle_remote_input(int n);
int handle_local_conn_request(char* onion_id, uint16_t port);
int handle_remote_conn_request();


//*********************************
//      THREAD FUNCTIONS
//*********************************
void* th_new_conn();
int th_new_input();
void*  th_main_loop();

#endif
