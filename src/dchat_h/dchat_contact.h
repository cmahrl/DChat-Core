#ifndef DCHAT_CONTACT_H
#define DCHAT_CONTACT_H

#include "dchat_types.h"

//*********************************
//       DCHAT PROTO FUNCTIONS
//*********************************
int send_contacts(dchat_conf_t* cnf, int n);
int receive_contacts(dchat_conf_t* cnf, dchat_pdu_t* pdu);
int check_duplicates(dchat_conf_t* cnf, int n);


//*********************************
//       CONVERSION FUNCTIONS
//*********************************
char* contact_to_string(contact_t* contact);
int string_to_contact(contact_t* contact, char* string);


//*********************************
//         MISC FUNCTIONS
//*********************************
int realloc_contactlist(dchat_conf_t* cnf, int newsize);
int add_contact(dchat_conf_t* cnf, int fd, struct sockaddr_storage* ss);
int del_contact(dchat_conf_t* cnf, int n);
int find_contact(dchat_conf_t* cnf, contact_t* contact, int begin);


#endif
