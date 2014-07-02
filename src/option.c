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


/** @file option.c
 * This file is used to store and handle command line options.
 */

#include <string.h>
#include <stdlib.h>

#include "dchat_h/option.h"
#include "dchat_h/log.h"


/**
 * Iterates the given command line options and crafts a
 * getopt short options string.
 * @param options Pointer to command line options array
 * @param size Size of command line options array
 * @return String containing all short options (inclusive
 * options paramters like ":" to specify a required argument)
 */
char*
get_short_options(cli_option_t* options, int size)
{
    char* opt_str = malloc(size * 2 + 1); // max. possible string

    if (opt_str == NULL)
    {
        fatal("Memory allocation for short options failed!");
    }

    opt_str[0] = '\0';

    for (int i = 0; i < size; i++)
    {
        // append short options
        strncat(opt_str, &options[i].opt, 1);

        // getopt uses `:` to specify a required
        // argument for an option
        if (options[i].mandatory_argument)
        {
            strncat(opt_str, ":", 1);
        }
    }

    return opt_str;
}


/**
 * Iterates the given command line options and crafts a
 * getopt long options array of the type struct option.
 * @param options Pointer to command line options array
 * @param size Size of command line options array
 * @return An option struct containing all long options
 */
struct option*
get_long_options(cli_option_t* options, int size)
{
    struct option* long_options = malloc(size * sizeof(struct option));

    if (long_options == NULL)
    {
        fatal("Memory allocation for long options failed!");
    }

    for (int i = 0; i < size; i++)
    {
        // set field values
        long_options[i].name = options[i].long_opt;
        long_options[i].has_arg = options[i].mandatory_argument;
        long_options[i].flag = 0;
        long_options[i].val = options[i].opt;
    }

    return long_options;
}

