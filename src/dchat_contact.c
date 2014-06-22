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


/** @file dchat_contact.c
 *  This file contains functions concerning dchat contacts for adding, deleting,
 *  searching, sending and receiving contacts.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "dchat_h/dchat_contact.h"
#include "dchat_h/dchat_types.h"
#include "dchat_h/dchat_decoder.h"
#include "dchat_h/dchat.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"


/**
 *  Sends local contactlist to a contact.
 *  Sends all known contacts stored in the contactlist within the global config
 *  in form of a "control/discover" PDU to the given contact.
 *  @param cnf Pointer to global configuration holding the contactlist
 *  @param n   Index of contact to whom we send our contactlist (excluding him)
 *  @return amount of bytes that have been written as content, -1 on error
 */
int
send_contacts(dchat_conf_t* cnf, int n)
{
    dchat_pdu_t pdu;    // pdu with contact information
    char* contact_str;  // pointer to a string representation of a contact
    int i;
    int ret;            // return value
    int pdu_len = 0;    // total content length of pdu-packet that will be sent
    contact_t* contact; // contact that will be converted to a string
    // zero out new pdu and set initial dchat headers
    memset(&pdu, 0, sizeof(pdu));
    pdu.content_type = CT_CTRL_DISC;
    pdu.content_length = 0;
    pdu.listen_port = cnf->me.lport;

    // iterate through our contactlist
    for (i = 0; i < cnf->cl.cl_size; i++)
    {
        // except client n to whom we sent our contactlist
        if (n == i)
        {
            continue;
        }

        // temporarily point to a contact
        contact = &cnf->cl.contact[i];

        // if its not an empty contact slot and contact is not temporary (has not sent "control/discover" yet)
        if (contact->lport != 0)
        {
            // convert contact to a string
            if ((contact_str = contact_to_string(contact)) == NULL)
            {
                log_msg(LOG_WARN, "send_contacts() - Could not send contact");
                ret = -1;
                continue;
            }

            // (re)allocate memory for pdu for contact string
            pdu.content = realloc(pdu.content, pdu_len + strlen(contact_str) + 1);
            pdu.content[pdu_len] = '\0'; // set the first byte to \0.. used for strcat

            // could not allocate memory for content
            if (pdu.content == NULL)
            {
                log_msg(LOG_ERR, "send_contacts() - Could not allocate memory");
                ret = -1;
            }
            else
            {
                // add contact information to content
                strncat(pdu.content, contact_str, strlen(contact_str));
                // increase size of pdu content-length
                pdu_len += strlen(contact_str);
            }

            free(contact_str);
        }
    }

    // set content-length of this pdu
    pdu.content_length = pdu_len;

    // write pdu inkluding all addresses of our contacts
    if ((ret = write_pdu(cnf->cl.contact[n].fd, &pdu)) == -1)
    {
        log_msg(LOG_ERR, "send_contacts() failed - Could not allocate memory");
        ret = -1;
    }

    if (pdu.content_length != 0)
    {
        free(pdu.content);
    }

    return ret;
}


/**
 *  Contacts transferred via PDU will be added to the contactlist.
 *  Parses the contact information stored in the given PDU, connects to this remote client
 *  , if this client is unknown, the local contactlist within the global config will be sent
 *  to him. Finally the remote client is added as contact to the local contactlist.
 *  For every parsed contact information, this procedure is repeated.
 *  @param cnf Pointer to global configuration holding the contactlist
 *  @param pdu PDU with the contact information in its content
 *  @return amount of new contacts added to the contactlist, -1 on error
 */
int
receive_contacts(dchat_conf_t* cnf, dchat_pdu_t* pdu)
{
    contact_t contact;
    int ret = 0;            // return value
    int new_contacts = 0;   // stores how many new contacts have been received
    int known_contacts = 0; // stores how many known contacts have been received
    int line_begin = 0;     // offset of content of given pdu
    int line_end = 0;       // offset (end) of content of given pdu
    char* line;             // contact string

    // as long as the line_end index is lower than content-length
    while (line_end < pdu->content_length)
    {
        line_begin = line_end;

        // extract a line from the content of the pdu
        if ((line_end = get_content_part(pdu, line_begin, '\n', &line)) == -1)
        {
            log_msg(LOG_ERR, "receive_contacts(): Could not extract line from PDU");
            ret = -1;
            break;
        }

        // parse line ane make string to contact
        if (string_to_contact(&contact, line) == -1)
        {
            log_msg(LOG_WARN, "receive_contacts(): Could not convert string to contact");
            ret = -1;
            continue;
        }

        // if parsed contact is unknown
        if (find_contact(cnf, &contact, 0) == -2)
        {
            // increment new contacts counter
            new_contacts++;

            // connect to new contact, add him as contact, and send contactlist to him
            if (handle_local_conn_request(cnf, &contact.stor) == -1)
            {
                log_msg(LOG_WARN,
                        "receive_contacts(): Could not execute connection request successfully");
            }
        }
        else
        {
            // we found parsed contact in contactlist -> increment known contacts counter
            known_contacts++;
        }

        // increment end of line index, to point to the beginning of the next line
        line_end++;
    }

    return ret != -1 ? new_contacts : -1;
}


/**
 *  Checks the local contactlist for duplicates.
 *  Checks if there are duplicate contacts in the contactlist. Contacts
 *  with the same listening port and ip address are considered as duplicate.
 *  If there are duplicates, the index of the contact which should be deleted
 *  will be returned. This function implements the duplicate detection
 *  mechanismn of the DChat protocol. Therefore for further information read
 *  the DChat protocol specification for detecting and removing duplicates.
 *  @see find_contact()
 *  @param cnf Pointer to global configuration holding the contactlist
 *  @param n   Index of contact to check for duplicates
 *  @return index of duplicate, -1 if there are no duplicates, -2 on error
 */
int
check_duplicates(dchat_conf_t* cnf, int n)
{
    int fst_oc;              // index of first occurance of contact
    int sec_oc;              // index of duplicate
    contact_t* temp;         // temporay contact variable
    int connect_contact = 0; // index of contact to whom we connected to
    int accept_contact = 0;  // index of contact from whom we accepted a connection
    // check if given contact is in the contactlist
    fst_oc = find_contact(cnf, &cnf->cl.contact[n], 0);

    // contact is this client
    if(fst_oc == -1){
        return n;
    }

    if (fst_oc == -2)
    {
        return -2; // error contact not in list
    }

    // check if given contact is in the contactlist a second time
    sec_oc = find_contact(cnf, &cnf->cl.contact[n], fst_oc + 1);

    if (sec_oc == -2)
    {
        return -1; // no duplicate contact
    }

    // extract port of sockaddr_storage structure
    temp = &cnf->cl.contact[fst_oc];

    if (ip_version(&temp->stor) == 4)
    {
        if (temp->lport == ntohs(((struct sockaddr_in*) &temp->stor)->sin_port))
        {
            // if listening port equlals port of sockaddr_storage
            // then the client has connected to the other client
            connect_contact = fst_oc;
            // therefore the other contact in the contactlist has
            // been accepted
            accept_contact = sec_oc;
        }
        else
        {
            // otherwise it is the other way round
            connect_contact = sec_oc;
            accept_contact = fst_oc;
        }
    }
    // do the same for ipv6
    else if (ip_version(&temp->stor) == 6)
    {
        if (temp->lport == ntohs(((struct sockaddr_in6*) &temp->stor)->sin6_port))
        {
            connect_contact = fst_oc;
            accept_contact = sec_oc;
        }
        else
        {
            connect_contact = sec_oc;
            accept_contact = fst_oc;
        }
    }

    // compare IP and LPORT
    // the client with the greater tuple will close the "connect" connection
    // the client with the lower tuple will close the "accept" connection
    if (ip_version(&cnf->me.stor) == 4 && ip_version(&cnf->cl.contact[n].stor) == 4)
    {
        in_addr_t me_addr = ((struct sockaddr_in*) &cnf->me.stor)->sin_addr.s_addr;
        in_addr_t n_addr = ((struct sockaddr_in*)
                            &cnf->cl.contact[n].stor)->sin_addr.s_addr;

        // if local ip address is greater than the remote one
        // than the index of the  contact, who got added because of a "connect",
        // will be returned
        if (me_addr > n_addr)
        {
            return connect_contact;
        }
        // otherwise it is the other way round
        else if (me_addr < n_addr)
        {
            return accept_contact;
        }
        // if ip addresses are equal, do the same for the listening port
        else if (cnf->me.lport > cnf->cl.contact[n].lport)
        {
            return connect_contact;
        }
        else if (cnf->me.lport < cnf->cl.contact[n].lport)
        {
            return accept_contact;
        }
        else
        {
            log_msg(LOG_ERR, "Contact is stored twice in contactlist");
            return accept_contact;
        }
    }
    // do the same for ipv6 (see above)
    else if (ip_version(&cnf->me.stor) == 6 &&
             ip_version(&cnf->cl.contact[n].stor) == 6)
    {
        int mem_ret;
        struct in6_addr me_addr = ((struct sockaddr_in6*) &cnf->me.stor)->sin6_addr;
        struct in6_addr n_addr = ((struct sockaddr_in6*)
                                  &cnf->cl.contact[n].stor)->sin6_addr;
        mem_ret = memcmp(&me_addr, &n_addr, sizeof(struct in6_addr));

        if (mem_ret > 0)
        {
            return connect_contact;
        }
        else if (mem_ret < 0)
        {
            return accept_contact;
        }
        else if (cnf->me.lport > cnf->cl.contact[n].lport)
        {
            return connect_contact;
        }
        else if (cnf->me.lport < cnf->cl.contact[n].lport)
        {
            return accept_contact;
        }
        else
        {
            log_msg(LOG_ERR, "Contact is stored twice in contactlist");
            return accept_contact;
        }
    }
    // ipv6 rules ipv4
    else if (ip_version(&cnf->me.stor) == 6 &&
             ip_version(&cnf->cl.contact[n].stor) == 4)
    {
        return connect_contact;
    }
    // see above
    else if (ip_version(&cnf->me.stor) == 4 &&
             ip_version(&cnf->cl.contact[n].stor) == 6)
    {
        return accept_contact;
    }
    else
    {
        log_msg(LOG_ERR, "Incompatible ip address format");
        return -2;
    }
}


/**
 *  Converts a contact into a string.
 *  This function converts a contact into a string representation. The string
 *  of the contact will then be returned and should be freed.
 *  @param contact Pointer to contact that should be converted to a string
 *  @return String representation of contact, NULL on error
 */
char*
contact_to_string(contact_t* contact)
{
    char* contact_str; // pointer to string repr. of contact
    char addr_str[INET6_ADDRSTRLEN + 1]; // ip address of contact
    char port_str[MAX_INT_STR + 1]; // max. characters of an int
    struct sockaddr_storage* client_addr; // socket address of contact
    int ipv; // ip version used
    int contact_len; // length of contact structure
    // convert ip to a string
    client_addr = &contact->stor;

    if ((ipv = ip_version(client_addr)) == 4)
    {
        // get ip4 string
        inet_ntop(
            AF_INET,
            &(((struct sockaddr_in*) client_addr)->sin_addr),
            addr_str,
            INET_ADDRSTRLEN
        );
    }
    else if (ipv == 6)
    {
        // get ip6 string
        inet_ntop(
            AF_INET6,
            &(((struct sockaddr_in6*) client_addr)->sin6_addr),
            addr_str,
            INET6_ADDRSTRLEN
        );
    }
    else
    {
        log_msg(LOG_ERR, "Contact has an invalid ip address");
        return NULL;
    }

    // convert port to a string
    snprintf(port_str, MAX_INT_STR, "%u", contact->lport);
    port_str[5] = '\0';
    // +4 for two " ", \n and \0
    contact_len = strlen(addr_str) + strlen(port_str) + 4;

    // allocate memory for the contact string repr.
    if ((contact_str = malloc(contact_len)) == NULL)
    {
        log_msg(LOG_ERR, "contact_to_string() failed - Could not allocate memory");
        return NULL;
    }

    // first byte has to be '\0', otherwise strcat is undefined
    contact_str[0] = '\0';
    // create contact string like this:
    // <ip address> <port>\n
    // (see: DChat Protocol - Contact Exchange
    strncat(contact_str, addr_str, strlen(addr_str));
    strncat(contact_str, " ", 1);
    strncat(contact_str, port_str, strlen(port_str));
    strncat(contact_str, "\n", 1);
    return contact_str;
}


/**
 *  Converts a string into a contact.
 *  Converts a string containing contact information into a contact structure.
 *  The string has to be in the form of: <ip> <port>\n
 *  Further details can be found in the DChat protocol specification
 *  @see contact_to_string()
 *  @param contact: Pointer to contact that should be converted to a string
 *  @return 0 if conversion was successful, -1 on error
 */
int
string_to_contact(contact_t* contact, char* string)
{
    char* ip;       // ip address string (splitted from line)
    char* port;     // port string (splitted from line)
    char* save_ptr; // pointer for strtok_r

    // split ip from string
    if ((ip = strtok_r(string, " ", &save_ptr)) == NULL)
    {
        log_msg(LOG_ERR, "string_to_contact() - Could not parse ip address");
        return -1;
    }

    // split port from string
    if ((port = strtok_r(NULL, "\n", &save_ptr)) == NULL)
    {
        log_msg(LOG_ERR, "string_to_contact() - Could not parse port");
        return -1;
    }

    // could line be splittet into ip and port?
    if (ip != NULL && port != NULL)
    {
        // convert port string to integer
        contact->lport = atoi(port);

        // ipv4 address?
        if (strchr(ip, '.') != NULL)
        {
            // convert ipv4 address and port
            if (inet_pton(AF_INET, ip,
                          &(((struct sockaddr_in*) &contact->stor)->sin_addr)) == 1)
            {
                // set ip and port in scocketaddress
                ((struct sockaddr_in*) &contact->stor)->sin_family = AF_INET;
                ((struct sockaddr_in*) &contact->stor)->sin_port = htons(atoi(port));
            }
            else
            {
                log_msg(LOG_WARN, "string_to_contact() - Corrupt ip4 address");
                return -1;
            }
        }
        // ipv6 address?
        else if (strchr(ip, ':') != NULL)
        {
            // convert ipv4 address and port
            if (inet_pton(AF_INET6, ip,
                          &(((struct sockaddr_in6*) &contact->stor)->sin6_addr)) == 1)
            {
                // set ip and port in scocketaddress
                ((struct sockaddr_in6*) &contact->stor)->sin6_family = AF_INET6;
                ((struct sockaddr_in6*) &contact->stor)->sin6_port = htons(atoi(port));
            }
            else
            {
                log_msg(LOG_WARN, "string_to_contact() - Corrupt ip6 address");
                return -1;
            }
        }
        else
        {
            log_msg(LOG_WARN, "string_to_contact() - Corrupt ip '%s'", ip);
            return -1;
        }
    }

    return 0;
}


/**
 *  Resizes the contactlist.
 *  Function to resize the contactlist to a given size. Old contacts are copied to the new
 *  resized contact list if they fit in it
 *  @param cnf Global config structure holding the contactlist
 *  @param newsize New size of the contactlist
 *  @return 0 on success, -1 on error, -2 if the new size is lower than the amount of contacts
 *          actually stored within the contactlist
 */
int
realloc_contactlist(dchat_conf_t* cnf, int newsize)
{
    int i, j = 0;
    contact_t* new_contact_list;
    contact_t* old_contact_list;

    // size may not be lower than 1 and not be lower than the amount of contacts actually used
    if (newsize < 1 || newsize < cnf->cl.used_contacts)
    {
        //FIXME: why -2??
        return -2;
    }

    // reserve memory for new contactlist
    if ((new_contact_list = malloc(newsize * sizeof(contact_t))) == NULL)
    {
        log_msg(LOG_ERR, "realloc_contactlist() failed - Could not allocate memory");
        return -1;
    }

    // pointer to beginning of old contactlist
    old_contact_list = cnf->cl.contact;
    // zero out new contactlist
    memset(new_contact_list, 0, newsize * sizeof(contact_t));

    // copy contacts to new contactlist
    for (i = 0; i < cnf->cl.cl_size; i++)
    {
        if (old_contact_list[i].fd)
        {
            memcpy(new_contact_list + j, old_contact_list + i, sizeof(contact_t));
            j++;
        }
    }

    // set new size and point to new contactlist in the global config
    cnf->cl.cl_size = newsize;
    cnf->cl.contact = new_contact_list;
    // free old contactlist
    free(old_contact_list);
    return 0;
}


/**
 *  Adds a new contact to the local contactlist.
 *  The given socket descriptor and socket address of the remote client will
 *  be used to add a new contact to the contactlist holded by the global config.
 *  @param cnf Pointer to dchat_conf_t structure holding the contact list
 *  @param fd  Socket file descriptor of the new contact
 *  @param ss  Pointer to socket address of the new contact
 *  @return index of contact list, where new contact has been added or -1 if contact
 *          list is full
 */
int
add_contact(dchat_conf_t* cnf, int fd, struct sockaddr_storage* ss)
{
    int i;

    // if contactlist is full - resize it so that we can store more contacts in it
    if (cnf->cl.used_contacts == cnf->cl.cl_size)
    {
        if ((realloc_contactlist(cnf, cnf->cl.cl_size + INIT_CONTACTS)) < 0)
        {
            return -1;
        }
    }

    // search for an empty place to store the new contact
    for (i = 0; i < cnf->cl.cl_size; i++)
    {
        // if fd is 0 -> this place can be used to store a new contact
        if (!cnf->cl.contact[i].fd)
        {
            cnf->cl.contact[i].fd = fd;
            memcpy(&cnf->cl.contact[i].stor, ss, sizeof(*ss));
            cnf->cl.used_contacts++; // increase contact counter
            break;
        }
    }

    // return index where contact has been stored
    return i;
}


/**
 *  Deletes a contact from the local contactlist.
 *  Deletes a contact from the contact list holded by the global config.
 *  @param cnf Pointer to dchat_conf_t structure holding the contact list
 *  @param n   Index of customer in the customer list
 *  @return 0 on success, -1 if index is out of bounds
 */
int
del_contact(dchat_conf_t* cnf, int n)
{
    // is index 'n' a valid index?
    if ((n < 0) || (n >= cnf->cl.cl_size))
    {
        log_msg(LOG_ERR, "del_contact() - Index out of bounds '%d'", n);
        return -1;
    }

    // zero out the contact on index 'n'
    memset(&cnf->cl.contact[n], 0, sizeof(contact_t));
    // decrease contacts counter variable
    cnf->cl.used_contacts--;

    // if contacts counter has been decreased INIT_CONTACTS time, resize the contactlist to free
    // unused memory
    if ((cnf->cl.used_contacts == (cnf->cl.cl_size - INIT_CONTACTS)) &&
        cnf->cl.used_contacts != 0)
    {
        if ((realloc_contactlist(cnf, cnf->cl.cl_size - INIT_CONTACTS)) < 0)
        {
            return -1;
        }
    }

    return 0;
}


/**
 *  Searches a contact in the local contactlist.
 *  Searches for a contact in the contactlist holded within the global config
 *  and returns its index in the contactlist. To find a contact, its socket address
 *  and listening port will be used
 *  @param cnf     Pointer to global configuration holding the contactlist
 *  @param contact Pointer to contact to search for
 *  @param begin   Index where the search will begin in the contactlist
 *  @return index of contact, -1 if the contact represents ourself, -2 if not found
 */
int
find_contact(dchat_conf_t* cnf, contact_t* contact, int begin)
{
    int i;
    contact_t* c;

    // is begin a valid index?
    if (begin < 0 || begin >= cnf->cl.cl_size)
    {
        return -2;
    }

    // iterate through contactlist, but first check
    // if the contact represents ourself
    for (i = -1; i < cnf->cl.cl_size; i++)
    {
        // first check if the given contact matches ourself
        if (i == -1)
        {
            c = &cnf->me;
            i = begin - 1;
        }
        else
        {
            c = &cnf->cl.contact[i];
        }

        // dont check empty contacts or temporary contacts
        if (c->lport)
        {
            if (strcmp(contact_to_string(contact), contact_to_string(c)) == 0)
            {
                return i;
            }
        }
    }

    return -2; // not found
}
