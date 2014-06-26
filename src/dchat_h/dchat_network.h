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

// see: http://web.mit.edu/foley/www/TinFoil/src/tinfoil/TorLib.java 

#ifndef DCHAT_NETWORK_H
#define DCHAT_NETWORK_H

#include <stdint.h>


//*********************************
//     TOR SETTINGS 
//*********************************
#define ONION_ADDRLEN   22
#define TOR_PORT        9050
#define TOR_ADDR        "127.0.0.1"

//*********************************
//     SOCKS4a FIELDS
//*********************************
#define SOCKS_CONNECT   0x01
#define SOCKS_RESOLVE   0xF0
#define SOCKS_VERSION   0x04
#define SOCKS_DELIM     0x00
#define SOCKS_FAKEIP    0x01


/*!
 * Structure for a SOCKS4a PDU
 */
typedef struct socks4a_pdu
{
    uint8_t  version;   //!< SOCKS version (e.g 4/5)
    uint8_t  command;   //!< SOCKS command type (e.g. CONNECT, BIND, ...)
    uint16_t port;      //!< port of remote client
    uint32_t fakeip;    //!< invalid ip address (see: SOCKS4a protocol)
    uint8_t  delim;     //!< SOCKS delimiter
    char*    hostname;  //!< domain name of client to connect to
} socks4a_pdu_t;


int write_socks4a(int s, socks4a_pdu_t* pdu);
int read_socks4a(int s, socks4a_pdu_t* pdu);
char* parse_socks_status(unsigned char status);
int create_tor_socket(char* hostname, uint16_t rport);

#endif
