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


/** @file util.c
 *  This file contains several miscellanious utility functions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>


/**
 *  Define the maximum of two given integers.
 *  @param a First integer
 *  @param b Second integer
 *  @return either a or b, depending on which one is bigger
 */
int
max(int a, int b)
{
    return a > b ? a : b;
}


/**
 *  Checks if the given file exists.
 *  @param filename Filename of file to check
 *  @return 1 if file exists, 0 if not, -1 otherwise
 *  (check errno of fopen)
 */
int file_exists(char* filename)
{
    FILE* f = fopen(filename, "r");

    if (f == NULL)
    {
        if (errno == ENOENT)
        {
            return 0;
        }

        return -1;
    }

    fclose(f);
    return 1;
}


/**
 *  Removes leading whitespaces.
 *  @param value Null-terminated string
 *  @return Pointer to string whose leading spaces were removed or
 *  NULL (check errno of fopen)
 */
char*
remove_leading_spaces(char* value)
{
    if (value == NULL)
    {
        return NULL;
    }

    for (; isspace(*value); value++);

    return value;
}


/**
 *  Checks wether n successive bytes in the memory pointed by
 *  ptr are 0.
 *  @param ptr Pointer to memory
 *  @return 1 if every byte is zero, 0 otherwise
 */
int
iszero(void* ptr, int n )
{
    char* bptr = (char*)ptr;

    while ( n-- )
        if ( *bptr++ )
        {
            return 0;
        }

    return 1;
}
