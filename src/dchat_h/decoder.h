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


#ifndef DECODER_H
#define DECODER_H

#include "types.h"


//*********************************
//          LIMITS
//*********************************
#define MAX_CONTENT_LEN 4096
#define HDR_AMOUNT      8
#define CTT_AMOUNT      4


//*********************************
//          VERSION
//*********************************
#define DCHAT_V1 1.0


//*********************************
//         ID OF HEADER
//*********************************
#define HDR_ID_VER 0x01
#define HDR_ID_CTT 0x02
#define HDR_ID_CTL 0x03
#define HDR_ID_ONI 0x04
#define HDR_ID_LNP 0x05
#define HDR_ID_NIC 0x06
#define HDR_ID_DAT 0x07
#define HDR_ID_SRV 0x08


//*********************************
//         NAME OF HEADER
//*********************************
#define HDR_NAME_VER "DCHAT"
#define HDR_NAME_CTT "Content-Type"
#define HDR_NAME_CTL "Content-Length"
#define HDR_NAME_ONI "Host"
#define HDR_NAME_LNP "Listen-Port"
#define HDR_NAME_NIC "Nickname"
#define HDR_NAME_DAT "Date"
#define HDR_NAME_SRV "Server"


//*********************************
//         ID OF CONTENT-TYPE
//*********************************
#define CTT_ID_TXT 0x01
#define CTT_ID_BIN 0x02
#define CTT_ID_DSC 0x03
#define CTT_ID_RPY 0x04

#define CTT_MASK_ALL 0x05


//*********************************
//         NAME OF CONTENT-TYPE
//*********************************
#define CTT_NAME_TXT "text/plain"
#define CTT_NAME_BIN "application/octet"
#define CTT_NAME_DSC "control/discover"
#define CTT_NAME_RPY "control/replay"


//*********************************
//             MACRO
//*********************************
#define HEADER(ID, NAME, MAND, STR2PDU, PDU2STR) { ID, NAME, MAND, STR2PDU, PDU2STR }
#define CONTENT_TYPE(ID, NAME) { ID, NAME }


/*!
 * Structure of a DChat content-type.
 * Specifies content-type name and its id for internal
 * use in this program.
 */
typedef struct dchat_content_type
{
    int   ctt_id;
    char* ctt_name;
} dchat_content_type_t;


/*!
 * Structure of a DChat content-type.
 * Specifies content-type name and its id for internal
 * use in this program.
 */
typedef struct dchat_content_types
{
    dchat_content_type_t type[CTT_AMOUNT];
} dchat_content_types_t;


/*!
 * Structure of a DChat Header.
 * Specifies header name and how to parse the value
 * for this specific header.
 */
typedef struct dchat_header
{
    int   header_id;
    char* header_name;
    int   mandatory;
    int (*str_to_pdu)(char*, dchat_pdu_t*);
    int (*pdu_to_str)(dchat_pdu_t*, char**);
} dchat_header_t;


/*!
 * Structure that defines what kind of headers
 * DChatV1 has.
 */
typedef struct dchat_v1
{
    dchat_header_t header[HDR_AMOUNT];
} dchat_v1_t;


//*********************************
//        DECODE FUNCTIONS
//*********************************
int decode_header(dchat_pdu_t* pdu, char* line);
int read_line(int fd, char** line);
int read_pdu(int fd, dchat_pdu_t* pdu);


//*********************************
//        ENCODE FUNCTIONS
//*********************************
int encode_header(dchat_pdu_t* pdu, int header_id, char** headerline);
int write_pdu(int fd, dchat_pdu_t* pdu);


//*********************************
//        PARSING FUNCTIONS
//*********************************
int ver_str_to_pdu(char* value, dchat_pdu_t* pdu);
int ctt_str_to_pdu(char* value, dchat_pdu_t* pdu);
int ctl_str_to_pdu(char* value, dchat_pdu_t* pdu);
int oni_str_to_pdu(char* value, dchat_pdu_t* pdu);
int lnp_str_to_pdu(char* value, dchat_pdu_t* pdu);
int nic_str_to_pdu(char* value, dchat_pdu_t* pdu);
int dat_str_to_pdu(char* value, dchat_pdu_t* pdu);
int srv_str_to_pdu(char* value, dchat_pdu_t* pdu);

int ver_pdu_to_str(dchat_pdu_t* pdu, char** value);
int ctt_pdu_to_str(dchat_pdu_t* pdu, char** value);
int ctl_pdu_to_str(dchat_pdu_t* pdu, char** value);
int oni_pdu_to_str(dchat_pdu_t* pdu, char** value);
int lnp_pdu_to_str(dchat_pdu_t* pdu, char** value);
int nic_pdu_to_str(dchat_pdu_t* pdu, char** value);
int dat_pdu_to_str(dchat_pdu_t* pdu, char** value);
int srv_pdu_to_str(dchat_pdu_t* pdu, char** value);


//*********************************
//        INIT FUNCTIONS
//*********************************
int init_dchat_content_types(dchat_content_types_t* ctt);
int init_dchat_v1(dchat_v1_t* proto);
int init_dchat_pdu(dchat_pdu_t* pdu, float version, int content_type,
                   char* onion_id,
                   int lport, char* nickname);
void init_dchat_pdu_content(dchat_pdu_t* pdu, char* content, int len);


//*********************************
//        MISC FUNCTIONS
//*********************************
int is_valid_termination(char* value);
int is_valid_version(float version);
int is_valid_content_type(int content_type);
int is_valid_content_length(int ctl);
int is_valid_nickname(char* nickname);
void free_pdu(dchat_pdu_t* pdu);
int get_content_part(dchat_pdu_t* pdu, int offset, char term, char** content);


#endif
