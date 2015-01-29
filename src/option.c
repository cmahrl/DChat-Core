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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dchat_h/option.h"
#include "dchat_h/decoder.h"
#include "dchat_h/contact.h"
#include "dchat_h/consoleui.h"
#include "dchat_h/util.h"


/**
 * Iterates the given command line options and crafts a
 * getopt short options string.
 * @param options Pointer to command line options structure
 * @return String containing all short options (inclusive
 * options paramters like ":" to specify a required argument)
 */
char*
get_short_options(cli_options_t* options)
{
    char* opt_str = malloc(CLI_OPT_AMOUNT * 2 + 1); // max. possible string

    if (opt_str == NULL)
    {
        ui_fatal("Memory allocation for short options failed!");
    }

    opt_str[0] = '\0';

    for (int i = 0; i < CLI_OPT_AMOUNT; i++)
    {
        // append short options
        strncat(opt_str, &options->option[i].opt, 1);

        // getopt uses `:` to specify a required
        // argument for an option
        if (options->option[i].mandatory_argument)
        {
            strncat(opt_str, ":", 1);
        }
    }

    return opt_str;
}


/**
 * Iterates the given command line options and crafts a
 * getopt long options array of the type struct option.
 * @param options Pointer to command line options structure
 * @return An option struct containing all long options
 */
struct option*
get_long_options(cli_options_t* options)
{
    struct option* long_options = malloc(CLI_OPT_AMOUNT * sizeof(struct option));

    if (long_options == NULL)
    {
        ui_fatal("Memory allocation for long options failed!");
    }

    for (int i = 0; i < CLI_OPT_AMOUNT; i++)
    {
        // set field values
        long_options[i].name = options->option[i].long_opt;
        long_options[i].has_arg = options->option[i].mandatory_argument;
        long_options[i].flag = 0;
        long_options[i].val = options->option[i].opt;
    }

    return long_options;
}


/**
 * Initializes a command line options structure with all available
 * DChat command line options and its corresponding option argument
 * parser functions.
 * @param options Pointer to cli options structure
 * @return 0 on success, -1 otherwise
 */
int
init_cli_options(cli_options_t* options)
{
    int temp_size;
    // possible commandline options
    cli_option_t temp[] =
    {
        OPTION(CLI_OPT_LONI, CLI_LOPT_LONI, CLI_OPT_ARG_LONI, 1, "Set the onion id of the local hidden service.", loni_parse),
        OPTION(CLI_OPT_NICK, CLI_LOPT_NICK, CLI_OPT_ARG_NICK, 1, "Set the nickname for this chat session.", nick_parse),
        OPTION(CLI_OPT_LPRT, CLI_LOPT_LPRT, CLI_OPT_ARG_LPRT, 0, "Set the local listening port.", lprt_parse),
        OPTION(CLI_OPT_RONI, CLI_LOPT_RONI, CLI_OPT_ARG_RONI, 0, "Set the onion id of the remote host to whom a connection should be established.", roni_parse),
        OPTION(CLI_OPT_RPRT, CLI_LOPT_RPRT, CLI_OPT_ARG_RPRT, 0, "Set the remote port of the remote host who will accept connections on this port.", rprt_parse),
        OPTION(CLI_OPT_HELP, CLI_LOPT_HELP, CLI_OPT_ARG_HELP, 0, "Display help.", help_parse)
    };
    temp_size = sizeof(temp) / sizeof(temp[0]);

    if (temp_size > CLI_OPT_AMOUNT)
    {
        return -1;
    }

    memset(options, 0, sizeof(*options));
    memcpy(options->option, &temp, sizeof(temp));
    return 0;
}


/**
 * Reads a configuration file located at CONFIG_PATH.
 * Reads a dchat configuration file located at CONFIG_PATH and sets read
 * values in the global config. All required commandline options are
 * supported and can be specified in the config file as they would have
 * been specified in the commandline. The only difference is that in
 * the config a option is specified like this: "<option> <argument>\n".
 * @param filepath Path to config file
 * @param required_set Amount of required options set and specified in
 * the config file
 * @return 0 on success, -1 if file could not be read (see: ERRNO of fopen)
 * or a value > 0 indicating the line in the config file where an error
 * occured
 */
int
read_conf(char* filepath, int* required_set)
{
    FILE* f;               // config file file stream
    int fd;                // config file file descriptor
    char* line;            // read line of config file
    char* opt;             // cli opt
    char* arg;             // cli opt argument
    char* delim = " ";     // cli opt delimiter
    int lctr  = 1;         // line counter
    int end;               // index of termination
    int found_opt;         // flag if read option has been found
    int error = 0;         // boolean if error occured
    cli_options_t options; // available command line options
    int ret;

    // init available options
    if (init_cli_options(&options) == -1)
    {
        ui_log(LOG_ERR, "Initialization of command line options failed!");
        return -1;
    }

    // open config file
    f = fopen(filepath, "r");

    if (f == NULL)
    {
        ui_log_errno(LOG_ERR, "Could not read file '%s'!", filepath);
        return -1;
    }

    // get file descriptor of file stream
    fd = fileno(f);

    // read confif file line by line
    while (read_line(fd, &line) > 0)
    {
        // split line to get option and option argument
        if ((opt = strtok_r(line, delim, &arg)) == NULL)
        {
            error = 1;
            break;
        }

        // skip spaces if there is an argument
        if (remove_leading_spaces(arg) != NULL)
        {
            // get index of termination char (\n or \r\n)
            if ((end = is_valid_termination(arg)) == -1)
            {
                error = 1;
                break;
            }

            // remove termination char
            arg[end] = '\0';
        }
        else
        {
            arg = "";
        }

        // check if read option is an supported/available option
        found_opt = 0;

        for (int i = 0; i < CLI_OPT_AMOUNT; i++)
        {
            // skip non mandatory options
            if (!options.option[i].mandatory_option)
            {
                continue;
            }

            if (!strcmp(opt, options.option[i].long_opt))
            {
                found_opt = 1;

                // parse option argument and set in global config
                // do not force override, if value is already set
                // in the global config
                if ((ret = options.option[i].parse_option(arg, 0)) == -1)
                {
                    error = 1;
                    break;
                }

                // increment counter of set required options
                // if the parsing function has set the options value
                // in the global conf
                if (!ret)
                {
                    (*required_set)++;
                }
            }
        }

        free(line);

        // raise error if read option is an unsupported option
        // or break loop on error
        if (!found_opt || error)
        {
            error = 1;
            break;
        }

        // increase line pointer
        lctr++;
    }

    fclose(f);

    if (error)
    {
        return lctr;
    }

    return 0;
}


/**
 * Parses the terminal command line argument string to a
 * local onion address
 * and store it in the global dchat configuration.
 * @param value Pointer to argument string
 * @param force If set parsed argument string will override
 *              the corresponding settings in the global config
 * @return 0 on success, 1 nothing has been done or -1 on error.
 */
int
loni_parse(char* value, int force)
{
    if (!is_valid_onion(value))
    {
        return -1;
    }

    if (force || !is_valid_onion(_cnf->me.onion_id))
    {
        _cnf->me.onion_id[0] = '\0';
        strncat(_cnf->me.onion_id, value, ONION_ADDRLEN);
        return 0;
    }

    return 1;
}


/**
 * Parses the terminal command line argument string to a nickname
 * and stores it in the global dchat configuration.
 * @param value Pointer to argument string
 * @param force If set parsed argument string will override
 *              the corresponding settings in the global config
 * @return 0 on success, 1 nothing has been done or -1 on error.
 */
int
nick_parse(char* value, int force)
{
    if (!is_valid_nickname(value))
    {
        return -1;
    }

    if (force || !is_valid_nickname(_cnf->me.name))
    {
        _cnf->me.name[0] = '\0';
        strncat(_cnf->me.name, value, MAX_NICKNAME);
        return 0;
    }

    return 1;
}


/**
 * Parses the terminal command line argument string to a local
 * listening port
 * and stores it in the global dchat configuration.
 * @param value Pointer to argument string
 * @param force If set parsed argument string will override
 *              the corresponding settings in the global config
 * @return 0 on success, 1 nothing has been done or -1 on error.
 */
int
lprt_parse(char* value, int force)
{
    char* term;
    int lport = (int) strtol(value, &term, 10);

    if (!is_valid_port(lport) || *term != '\0')
    {
        return -1;
    }

    if (force || !is_valid_port(_cnf->me.lport))
    {
        _cnf->me.lport = lport;
        return 0;
    }

    return 1;
}


/**
 * Parses the terminal command line argument string to a remote
 * onion address and stores it in the global dchat configuration.
 * A fake contact will be created at the index 0 of the contactlist,
 * given that the contactlist is empty, otherwise if its size is 1
 * the onion address (remote address) will be set.
 * @param value Pointer to argument string
 * @param force If set parsed argument string will override
 *              the corresponding settings in the global config
 * @return 0 on success, 1 nothing has been done or -1 on error.
 */
int
roni_parse(char* value, int force)
{
    int n;

    if (_cnf->cl.used_contacts > 1)
    {
        return 1;
    }

    if (!is_valid_onion(value))
    {
        return -1;
    }

    if (!_cnf->cl.used_contacts)
    {
        n = add_contact(0); // create fake contact

        if (n != 0)
        {
            ui_log(LOG_ERR, "Creation of fake contact failed!");
            return -1;
        }
    }

    if (force || !is_valid_onion(_cnf->cl.contact[n].onion_id))
    {
        _cnf->cl.contact[n].onion_id[0] = '\0';
        strncat(_cnf->cl.contact[n].onion_id, value, ONION_ADDRLEN);
        return 0;
    }

    return 1;
}


/**
 * Parses the terminal command line argument string to a remote port
 * and stores it in the global dchat configuration.
 * A fake contact will be created at the index 0 of the contactlist,
 * given that the contactlist is empty, otherwise if its size is 1
 * the port (remote port) will be set.
 * @param value Pointer to argument string
 * @param force If set parsed argument string will override
 *              the corresponding settings in the global config
 * @return 0 on success, 1 nothing has been done or -1 on error.
 */
int
rprt_parse(char* value, int force)
{
    int n = 0;
    char* term;
    int rport = (int) strtol(optarg, &term, 10);

    if (_cnf->cl.used_contacts > 1)
    {
        return 1;
    }

    if (!is_valid_port(rport) || *term != '\0')
    {
        return -1;
    }

    if (!_cnf->cl.used_contacts)
    {
        n = add_contact(0); // create fake contact

        if (n != 0)
        {
            ui_log(LOG_ERR, "Creation of fake contact failed!");
            return -1;
        }
    }

    if (force || !is_valid_port(_cnf->cl.contact[n].lport))
    {
        _cnf->cl.contact[n].lport = rport;
        return 0;
    }

    return 1;
}


/**
 * Parses the terminal command line string and if it is the
 * help option, the usage of this program will be printed.
 * @param value Pointer to argument string
 * @param force If set parsed argument string will override
 *              the corresponding settings in the global config
 * @return 1 on success or -1 on error.
 */
int
help_parse(char* value, int force)
{
    cli_options_t options;

    if (init_cli_options(&options) == -1)
    {
        return -1;
    }

    usage(EXIT_SUCCESS, &options, "");
    return 1;
}
