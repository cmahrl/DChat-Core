#ifndef DCHAT_CMD_H
#define DCHAT_CMD_H

#include "dchat_types.h"


//*********************************
//     MAIN PARSING FUNCTION
//*********************************
int parse_cmd(dchat_conf_t* cnf, char* buf);


//*********************************
//       COMMAND FUNCTIONS
//*********************************
int parse_cmd_connect(dchat_conf_t* cnf, char* cmd);
void parse_cmd_help(void);
int parse_cmd_list(dchat_conf_t* cnf);

#endif
