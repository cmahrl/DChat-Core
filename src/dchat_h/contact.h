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


#ifndef CONTACT_H
#define CONTACT_H

#include "types.h"

//*********************************
//       DCHAT PROTO FUNCTIONS
//*********************************
int send_contacts(int n);
int receive_contacts(dchat_pdu_t* pdu);
int check_duplicates(int n);


//*********************************
//       CONVERSION FUNCTIONS
//*********************************
char* contact_to_string(contact_t* contact);
int string_to_contact(contact_t* contact, char* string);


//*********************************
//         MISC FUNCTIONS
//*********************************
int realloc_contactlist(int newsize);
int add_contact(int fd);
int del_contact(int n);
int find_contact(contact_t* contact, int begin);


#endif
