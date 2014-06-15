#ifndef DCHAT_H
#define DCHAT_H

#include <sys/socket.h>
#include "dchat_types.h"

//*********************************
//      INIT/DESTROY FUNCTIONS
//*********************************
int init(dchat_conf_t* cnf, char* interface, int acpt_port, char* nickname);
int init_global_config(dchat_conf_t* cnf, char* interface, int acpt_port,
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
void usage(char* app);

#endif
