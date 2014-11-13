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
int send_contacts(dchat_conf_t* cnf, int n);
int receive_contacts(dchat_conf_t* cnf, dchat_pdu_t* pdu);
int check_duplicates(dchat_conf_t* cnf, int n);


//*********************************
//       CONVERSION FUNCTIONS
//*********************************
char* contact_to_string(int log_fd, contact_t* contact);
int string_to_contact(int log_fd, contact_t* contact, char* string);


//*********************************
//         MISC FUNCTIONS
//*********************************
int realloc_contactlist(dchat_conf_t* cnf, int newsize);
int add_contact(dchat_conf_t* cnf, int fd);
int del_contact(dchat_conf_t* cnf, int n);
int find_contact(dchat_conf_t* cnf, contact_t* contact, int begin);


#endif
