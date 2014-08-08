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


#ifndef CMDINTERPRETER_H
#define CMDINTERPRETER_H

#include "types.h"
#include "option.h"


//*********************************
//          MISC
//*********************************
#define CMD_AMOUNT 3
#define CMD_PREFIX "/"


//*********************************
//        ID OF COMMAND
//*********************************
#define CMD_ID_HLP 0x01
#define CMD_ID_CON 0x02
#define CMD_ID_LST 0x03


//*********************************
//        NAME OF COMMAND
//*********************************
#define CMD_NAME_HLP CMD_PREFIX "help"
#define CMD_NAME_CON CMD_PREFIX "connect"
#define CMD_NAME_LST CMD_PREFIX "list"


//*********************************
//        ARG OF COMMAND
//*********************************
#define CMD_ARG_HLP ""
#define CMD_ARG_CON CLI_OPT_ARG_RONI " " CLI_OPT_ARG_RPRT
#define CMD_ARG_LST ""


//*********************************
//            MACRO
//*********************************
#define COMMAND(ID, NAME, ARG, FUNC) { ID, NAME, NAME " " ARG, FUNC }


/*!
 * Structure for a DChat in-chat command.
 */
typedef struct cmd
{
    int   cmd_id;
    char* cmd_name;
    char* syntax;
    int (*execute)(dchat_conf_t* cnf, char* arg);
} cmd_t;


/*!
 * Structure for DChat in-chat commands.
 */
typedef struct cmds
{
    cmd_t cmd[CMD_AMOUNT];
} cmds_t;

//*********************************
//     MAIN PARSING FUNCTION
//*********************************
int parse_cmd(dchat_conf_t* cnf, char* buf);


//*********************************
//       COMMAND FUNCTIONS
//*********************************
int hlp_exec(dchat_conf_t* cnf, char* arg);
int con_exec(dchat_conf_t* cnf, char* arg);
int lst_exec(dchat_conf_t* cnf, char* arg);


//*********************************
//       INIT FUNCTIONS
//*********************************
int init_cmds(cmds_t* cmds);


#endif
