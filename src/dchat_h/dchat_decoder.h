#ifndef DCHAT_DECODER_H
#define DCHAT_DECODER_H

#include "dchat_types.h"


//*********************************
//     LIMITS
//*********************************
#define MAX_CONTENT_LEN 4096

//*********************************
//     TYPE OF HEADER TO ENCODE
//*********************************
#define HDR_CONTENT_TYPE   1
#define HDR_CONTENT_LENGTH 2
#define HDR_LISTEN_PORT    3


//*********************************
//         CONTENT-TYPES
//*********************************
#define CT_TXT_PLAIN   1
#define CT_APPL_OCT    2
#define CT_CTRL_DISC   3
#define CT_CTRL_RPLY   4


//*********************************
//        DECODE FUNCTIONS
//*********************************
int decode_header(dchat_pdu_t* pdu, char* line);
int read_line(int fd, char** line);
int read_pdu(int fd, dchat_pdu_t** pdu);


//*********************************
//        ENCODE FUNCTIONS
//*********************************
char* encode_header(dchat_pdu_t* pdu, int header_id);
int write_line(int fd, char* buf);
int write_pdu(int fd, dchat_pdu_t* pdu);


//*********************************
//        MISC FUNCTIONS
//*********************************
void free_pdu(dchat_pdu_t* pdu);
int get_content_part(dchat_pdu_t* pdu, int offset, char term, char** content);

#endif
