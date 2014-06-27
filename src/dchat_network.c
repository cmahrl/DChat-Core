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


/** @file dchat_network.c
 *  This file contains core networking functions.
 */

#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include "dchat_h/dchat_network.h"
#include "dchat_h/log.h"


/**
 * Writes a SOCKS PDU to the given socket.
 * @param s   Socket where the PDU will be written to
 * @param pdu SOCKS PDU that will be written
 * @return 0 on success, -1 in case of error
 */
int
write_socks4a(int s, socks4a_pdu_t* pdu)
{
    // convert ip and port to network byte order
    uint16_t rport  = htons(pdu->port);
    uint32_t fakeip = htonl(pdu->fakeip);

    if (write(s, &pdu->version, 1) == -1)
    {
        return -1;
    }
    else if (write(s, &pdu->command, 1) == -1)
    {
        return -1;
    }
    else if (write(s, &rport, 2) == -1)
    {
        return -1;
    }
    else if (write(s, &fakeip, 4) == -1)
    {
        return -1;
    }
    else if (write(s, &pdu->delim, 1) == -1)
    {
        return -1;
    }
    else if (write(s, pdu->hostname, strlen(pdu->hostname)) == -1)
    {
        return -1;
    }
    else if (write(s, &pdu->delim, 1) == -1)
    {
        return -1;
    }

    return 0;
}


/**
 * Reads a SOCKS response PDU from the given socket.
 * @param s Socket to read from
 * @param
 */
int
read_socks4a(int s, socks4a_pdu_t* pdu)
{
    int ret;

    // read SOCKS4a response (see SOCKS4a protocol)
    if ((ret = read(s, pdu, 8)) <= 0)
    {
        //return ret;
    }

    // convert port and ip to host byte order
    pdu->port   = ntohs(pdu->port);
    pdu->fakeip = ntohl(pdu->fakeip);
    return ret;
}


/**
 * Parses given status and returns its corresponding status message.
 * @param status Status whose status message will be returned
 * @param Status message
 */
char*
parse_socks_status(unsigned char status)
{
    switch (status)
    {
        case 90:
            return "Request granted";

        case 91:
            return "Request rejected/failed - unknown reason";

        case 92:
            return "Request rejected: SOCKS server cannot connect to identd on the client";

        case 93:
            return "Request rejected: the client program and identd report different user-ids";

        default:
            return "Unknown status";
    }
}


/**
 * Creates a TOR socket.
 * This function creates a TOR socket by establishing a connection to the listening
 * address and port of the TOR client and sending a SOCKS connection request so that
 * a curcuit to the remote host will be created.
 * @param hostname The hostname of the destination
 * @param rport    The port to connect to
 * @return open socket whose traffic will be relayed through TOR or -1 in case of error
 */
int
create_tor_socket(char* hostname, uint16_t rport)
{
    int s;                 // tor socket
    struct sockaddr_in da; // destination address to connec to
    socks4a_pdu_t pdu;     // SOCKS request
    int ret;
    memset(&da, 0, sizeof(da));

    // socket address for connection to the TOR client
    if (inet_pton(AF_INET, TOR_ADDR, &da.sin_addr) != 1)
    {
        log_msg(LOG_ERR, "Invalid ip address '%s'!", TOR_ADDR);
        return -1;
    }

    da.sin_family = AF_INET;
    da.sin_port = htons(TOR_PORT);

    // connect to TOR client
    if ((s = connect_to((struct sockaddr*) &da)) == -1)
    {
        log_msg(LOG_ERR, "Could not create TOR socket!");
        return -1;
    }

    // craft SOCKS request pdu
    memset(&pdu, 0, sizeof(pdu));
    pdu.version = SOCKS_VERSION;
    pdu.command = SOCKS_CONNECT;
    pdu.port    = rport;
    pdu.fakeip  = SOCKS_FAKEIP;
    pdu.delim   = SOCKS_DELIM;
    pdu.hostname = hostname;

    // send connection request to TOR
    if (write_socks4a(s, &pdu) == -1)
    {
        log_errno(LOG_ERR, "Could not write SOCKS connection request!");
        return -1;
    }

    // read response code from TOR
    memset(&pdu, 0, sizeof(pdu));

    if ((ret = read_socks4a(s, &pdu)) == -1)
    {
        log_msg(LOG_ERR, "Could not read SOCKS connection response!");
        return -1;
    }

    if (!ret)
    {
        log_msg(LOG_ERR, "Connection to TOR client has been closed!");
        return -1;
    }

    if (pdu.command != 90)
    {
        log_msg(LOG_WARN,
                "TOR Connection to remote host failed. Status code: %d - '%s'", pdu.command,
                parse_socks_status(pdu.command));
        return -1;
    }

    return s;
}


/**
 *  Determines the address family of the given socket address structure.
 *  @param address Pointer to address to check the address family for
 *  @return 4 if AF_INET is used, 6 if AF_INET6 is used or -1 in every other case
 */
int
ip_version(struct sockaddr_storage* address)
{
    if (address->ss_family == AF_INET)
    {
        return 4;
    }
    else if (address->ss_family == AF_INET6)
    {
        return 6;
    }
    else
    {
        return -1;
    }
}


/**
 * Connects to a remote socket using the given socket address.
 * @param sa Pointer to initalized sockaddr structure.
 * @return file descriptor of new socket or -1 in case of error
 */
int
connect_to(struct sockaddr* sa)
{
    int s; // socket file descriptor

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        log_errno(LOG_ERR, "socket() failed in connect_to()");
        return -1;
    }

    if (connect(s, sa, sizeof(struct sockaddr_in)) == -1)
    {
        log_errno(LOG_ERR, "connect() failed");
        close(s);
        return -1;
    }

    return s;
}


/**
 * Checks wether the given port is a valid TCP port.
 * Valid ports are between 1 and 65536.
 * @return 1 if port is valid, 0 otherwise
 */
int
is_valid_port(int port)
{
    if(port > 0 && port < 65536)
    {
        return 1;
    }

    return 0;
}


/**
 * Checks wether the given onion-id is a valid onion address.
 * A valid onion address contains exactly 16 characters (excluding
 * the prefix) and has a `.onion` prefix.
 * @return 1 if onion-id is valid, 0 otherwise.
 */
int
is_valid_onion(char* onion_id)
{
    char* prefix;

    if(strlen(onion_id) != ONION_ADDRLEN)
    {
        return 0;
    }

    prefix = strchr(onion_id, '.');
    if(prefix == NULL)
    {
        return 0;
    }

    if(strcmp(prefix, ".onion") != 0)
    {
        return 0;
    }

    return 1;
}
