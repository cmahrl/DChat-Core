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
//     LIMITS
//*********************************
#define MAX_CONTENT_LEN 4096

//*********************************
//     TYPE OF HEADER TO ENCODE
//*********************************
#define HDR_CONTENT_TYPE   0x01
#define HDR_CONTENT_LENGTH 0x02
#define HDR_ONION_ID       0x03
#define HDR_LISTEN_PORT    0x04
#define HDR_NICKNAME       0x05


//*********************************
//         CONTENT-TYPES
//*********************************
#define CT_TXT_PLAIN   0x01
#define CT_APPL_OCT    0x02
#define CT_CTRL_DISC   0x03
#define CT_CTRL_RPLY   0x04
#define CT_ALL_MASK    0x05


//*********************************
//        DECODE FUNCTIONS
//*********************************
int decode_header(dchat_pdu_t* pdu, char* line);
int read_line(int fd, char** line);
int read_pdu(int fd, dchat_pdu_t* pdu);


//*********************************
//        ENCODE FUNCTIONS
//*********************************
char* encode_header(dchat_pdu_t* pdu, int header_id);
int write_line(int fd, char* buf);
int write_pdu(int fd, dchat_pdu_t* pdu);


//*********************************
//        INIT FUNCTIONS
//*********************************
int init_dchat_pdu(dchat_pdu_t* pdu, int content_type, char* onion_id,
                   int lport, char* nickname);
void init_dchat_pdu_content(dchat_pdu_t* pdu, char* content, int len);


//*********************************
//        MISC FUNCTIONS
//*********************************
int is_valid_content_type(int content_type);
int is_valid_nickname(char* nickname);
void free_pdu(dchat_pdu_t* pdu);
int get_content_part(dchat_pdu_t* pdu, int offset, char term, char** content);

#endif
