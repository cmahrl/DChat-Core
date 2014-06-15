#ifndef UTIL_H
#define UTIL_H

#include <netinet/in.h>
#include <limits.h>

//max. amount of chars for integer str representation
#define MAX_INT_STR ((CHAR_BIT * sizeof(int) - 1) / 3 + 2)


//*********************************
//         NETWORK FUNCTIONS
//*********************************
int ip_version(struct sockaddr_storage* addr);
int connect_to(struct sockaddr* sa);


//*********************************
//       PRINTING FUNCTIONS
//*********************************
void ansi_term_clear_line(int out_fd);
void ansi_term_cr(int out_fd);
void print_dchat_msg(char* msg, int out_fd);


//*********************************
//         MISC FUNCTIONS
//*********************************
int max(int a, int b);

#endif
