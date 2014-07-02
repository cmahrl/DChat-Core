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

#define OPTION(OPT, LONG_OPT, ARG, MAND, DESC)                \
    {OPT[0], LONG_OPT, ARG, !strlen(ARG) ? 0 : 1, MAND,            \
        !strlen(ARG) ? "    -"OPT ", --"LONG_OPT "\n           "DESC : \
        "    -"OPT ", --"LONG_OPT "="ARG "\n           "DESC}


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
} cli_option_t;


char* get_short_options(cli_option_t* options, int size);
struct option* get_long_options(cli_option_t* options, int size);

#endif
