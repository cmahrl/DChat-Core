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
