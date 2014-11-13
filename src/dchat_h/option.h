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


#ifndef OPTION_H
#define OPTION_H

#include <getopt.h>

#include "types.h"


//*********************************
//            MISC
//*********************************
#define CLI_OPT_AMOUNT 6

//*********************************
//  COMMAND LINE OPTIONS (SHORT)
//*********************************
#define CLI_OPT_LONI "s"
#define CLI_OPT_NICK "n"
#define CLI_OPT_LPRT "l"
#define CLI_OPT_RONI "d"
#define CLI_OPT_RPRT "r"
#define CLI_OPT_HELP "h"


//*********************************
//  COMMAND LINE OPTIONS (LONG)
//*********************************
#define CLI_LOPT_LONI "lonion"
#define CLI_LOPT_NICK "nickname"
#define CLI_LOPT_LPRT "lport"
#define CLI_LOPT_RONI "ronion"
#define CLI_LOPT_RPRT "rport"
#define CLI_LOPT_HELP "help"


//*********************************
//     NAME OF OPTION ARGUMENT
//*********************************
#define CLI_OPT_ARG_LONI "ONIONID"
#define CLI_OPT_ARG_NICK "NICKNAME"
#define CLI_OPT_ARG_LPRT "LOCALPORT"
#define CLI_OPT_ARG_RONI "REMOTEONIONID"
#define CLI_OPT_ARG_RPRT "REMOTEPORT"
#define CLI_OPT_ARG_HELP ""


//*********************************
//            MACROS
//*********************************
#define OPTION(OPT, LONG_OPT, ARG, MAND, DESC, FUNC)                \
    { OPT[0], LONG_OPT, ARG, !strlen(ARG) ? 0 : 1, MAND,            \
        !strlen(ARG) ? "    -"OPT ", --"LONG_OPT "\n           "DESC : \
        "    -"OPT ", --"LONG_OPT "="ARG "\n           "DESC, FUNC }


/*!
 * Structure for a command line option
 */
typedef struct cli_option
{
    char  opt;                //!< short option
    char* long_opt;           //!< long option
    char* arg;                //!< argument
    int   mandatory_argument; //!< argument is mandatory for option
    int   mandatory_option;   //!< option is mandatory
    char* description;        //!< description of option
    int   (*parse_option)(dchat_conf_t*, char*, int); //!< option parser
} cli_option_t;


/*!
 * Structure for available command line options
 */
typedef struct cli_options
{
    cli_option_t option[CLI_OPT_AMOUNT];
} cli_options_t;


//*********************************
//       INIT FUNCTIONS
//*********************************
char* get_short_options(int log_fd, cli_options_t* options);
struct option* get_long_options(int log_fd, cli_options_t* options);
int init_cli_options(cli_options_t* options);
int read_conf(dchat_conf_t* cnf, char* filepath, int* required_set);


//*********************************
//      CMD PARSING FUNCTIONS
//*********************************
int loni_parse(dchat_conf_t* cnf, char* value, int force);
int nick_parse(dchat_conf_t* cnf, char* value, int force);
int lprt_parse(dchat_conf_t* cnf, char* value, int force);
int roni_parse(dchat_conf_t* cnf, char* value, int force);
int rprt_parse(dchat_conf_t* cnf, char* value, int force);
int help_parse(dchat_conf_t* cnf, char* value, int force);

#endif
