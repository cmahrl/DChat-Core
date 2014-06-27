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


/** @file dchat_decoder.c
 *  This file contains core functions to decode and encode DChat Packet Data Units
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dchat_h/dchat_decoder.h"
#include "dchat_h/dchat_network.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"


/**
 *  Decodes a string into a DChat header.
 *  Attempts to decode the given \n terminated line and sets corresponding header attributes
 *  in the given pdu.
 *  @param pdu  Pointer to PDU structure where header attributes will be set
 *  @param line Line to parse for dchat-headers; must be \n terminated
 *  @return 0 if line is a dchat header, -1 otherwise
 */
int
decode_header(dchat_pdu_t* pdu, char* line)
{
    char* save_ptr;     // used for strtok and strtol
    char* key;          // header key (e.g. Content-Type)
    char* value;        // header value (e.g. text/plain)
    char* term;         // pointer to terminating chars of the line (e.g "\r\n")
    char* delim = ":";  // delimiter char
    int cmp_offset = 0; // header value offset
    int len;

    // split line: header format -> key:value
    if ((key = strtok_r(line, delim, &save_ptr)) == NULL)
    {
        return -1;
    }
    else if ((value = strtok_r(NULL, delim, &save_ptr)) == NULL)
    {
        return -1;
    }
    else
    {
        // first character must be a whitespace
        if (strncmp(value, " ", 1) != 0)
        {
            return -1;
        }
        else
        {
            cmp_offset++;    // increase offset
        }

        // check header type
        if (strcmp(key, "Content-Type") == 0)
        {
            // is offset within range?
            if (cmp_offset >= strlen(value))
            {
                return -1;
            }

            //FIXME: define header values as PPC
            //FIXME: define function to deterime EOL and index
            // check value type
            if (!strncmp(&value[cmp_offset], "text/plain", 10))
            {
                pdu->content_type = CT_TXT_PLAIN;
                cmp_offset += 10;
            }
            else if (!strncmp(&value[cmp_offset], "application/octet", 17))
            {
                pdu->content_type = CT_APPL_OCT;
                cmp_offset += 17;
            }
            else if (!strncmp(&value[cmp_offset], "control/discover", 16))
            {
                pdu->content_type = CT_CTRL_DISC;
                cmp_offset += 16;
            }
            else if (!strncmp(&value[cmp_offset], "control/replay", 14))
            {
                pdu->content_type = CT_CTRL_RPLY;
                cmp_offset += 14;
            }
            else
            {
                return -1;
            }

            // is offset within range?
            if (cmp_offset >= strlen(value))
            {
                return -1;
            }

            term = &value[cmp_offset];
        }
        else if (strcmp(key, "Content-Length") == 0)
        {
            // convert string to int
            pdu->content_length = (int) strtol(value, &save_ptr, 10);

            // check if its a valid content-length
            if (pdu->content_length < 0 || pdu->content_length > MAX_CONTENT_LEN)
            {
                return -1;
            }

            term = save_ptr;
        }
        else if (strcmp(key, "Onion-ID") == 0)
        {
            len = strlen(&value[cmp_offset]) - 1;   // skip \n

            if (value[cmp_offset + len - 1] == '\r') // skip \r
            {
                len--;
            }

            /*pdu->onion_id = malloc(len + 1);

            if (pdu->onion_id == NULL)
            {
                log_errno(LOG_ERR, "decode_header(): Could not allocate memory");
                return -1;
            }
            */
            pdu->onion_id[0] = '\0';

            if (len != ONION_ADDRLEN)
            {
                return -1;
            }
            //FIXME: check valid onion_id
            else
            {
                // copy onion address bytes
                strncat(pdu->onion_id, &value[cmp_offset], ONION_ADDRLEN);
            }

            // check if value string has been terminated properly
            term = &value[strlen(value) - 1]; // must be \n
        }
        else if (strcmp(key, "Listen-Port") == 0)
        {
            // convert string to int
            pdu->lport = (int) strtol(value, &save_ptr, 10);

            // check if it is a valid port
            if (pdu->lport < 1 || pdu->lport > 65535)
            {
                return -1;
            }

            term = save_ptr;
        }
        else if (strcmp(key, "Nickname") == 0)
        {
            len = strlen(&value[cmp_offset]) - 1;   // skip \n

            if (value[cmp_offset + len - 1] == '\r') // skip \r
            {
                len--;
            }

            /*pdu->nickname = malloc(len + 1);

            if (pdu->nickname == NULL)
            {
                log_errno(LOG_ERR, "decode_header(): Could not allocate memory");
                return -1;
            }*/

            pdu->nickname[0] = '\0';

            if (len <= MAX_NICKNAME)
            {
                // copy whole nickname without (\r)\n
                strncat(pdu->nickname, &value[cmp_offset], len);
            }
            else
            {
                // copy MAX_NICKNAME bytes of nickname without (\r)\n
                strncat(pdu->nickname, &value[cmp_offset], MAX_NICKNAME);
            }

            // check if value string has been terminated properly
            term = &value[strlen(value) - 1]; // must be \n
        }
        else
        {
            return -1;
        }

        // check if value is terminated properly
        if (!strcmp(term, "\r\n") || !strcmp(term, "\n"))
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
}


/**
 *  Read a line terminated with \\n from a file descriptor.
 *  Reads a line from the given file descriptor until \\n is found.
 *  @param fd   File descriptor to read from
 *  @param line Double pointer used for dynamic memory allocation since
 *              characters will be stored on the heap.
 *  @return: length of bytes read, 0 on EOF, -1 on error
 */
int
read_line(int fd, char** line)
{
    char* linep;        // line pointer
    char* tmp = NULL;   // used for realloc
    int ret;            // return value
    int len = 0;        // current length of string
    int max_line = 100; // realloc size
    // allocate memory for user input
    *line = malloc(max_line);

    if (*line == NULL)
    {
        return -1;
    }

    // point to beginning
    linep = *line;

    // until \n is found
    while (1)
    {
        // have 99 characters been read (excluding \0)?
        if ((len + 1) % max_line == 0)
        {
            // realloc memory for line
            tmp = realloc(*line, max_line *= 2);

            if (tmp == NULL)
            {
                log_errno(LOG_ERR, "realloc failed in read_line");
                free(*line);
                return -1;
            }
            else
            {
                *line = tmp;
                linep = *line + len; // get actual position in string after realloc
            }
        }

        // read 1 character from file descriptor
        if ((ret = read(fd, linep, 1)) == -1)
        {
            free(*line);
            return -1;
        }
        // EOF
        else if (ret == 0)
        {
            free(*line);
            return 0;
        }

        // line end
        if (*(linep) == '\n')
        {
            *(linep + 1) = '\0'; // Terminate
            return len + 1;
        }

        len++; // increase size of string
        linep++; // /increase line pointer
    }
}


/**
 *  Read a whole DChat PDU from a file descriptor.
 *  Read linewise from a file descriptor to form a DChat protocol data unit.
 *  Information read from the file descriptor will be stored in this pdu.
 *  @param fd  File descriptor to read from
 *  @param pdu Pointer to a PDU structure whose headers will be filled.
 *  @return amount of bytes read in total if a protocol data unit has been read successfully, 0 on EOF ,
 *  -1 on error
 */
int
read_pdu(int fd, dchat_pdu_t* pdu)
{
    char* line;     // line read from file descriptor
    char* contentp; // content pointer
    int ret;        // return value
    int b;          // amount of bytes read as content
    int len = 0;    // amount of bytes read in total

    // zero out structure
    memset(pdu, 0, sizeof(*pdu));

    // read each line of the received pdu
    if ((ret = read_line(fd, &line)) == -1 || !ret)
    {
        return ret;
    }

    // first line has to be "DCHAT: 1.0"
    if (strcmp(line, "DCHAT: 1.0\n") != 0 && strcmp(line, "DCHAT: 1.0\r\n") != 0)
    {
        free(line);
        return -1;
    }

    len += strlen(line);
    free(line);

    // read header lines from file descriptors, until
    // an empty line is received
    while (1)
    {
        // read line: -1 = error, 0 = EOF
        if ((ret = read_line(fd, &line)) == -1 || ret == 0)
        {
            free(line);
            return ret;
        }

        len += strlen(line);

        // decode read line as header
        if ((ret = decode_header(pdu, line)) == -1)
        {
            // if line is not a header, it must be an empty line
            if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
            {
                break; // All headers have been read
            }
            else
            {
                free(line);
                return -1;
            }
        }

        free(line);
    }

    // has content type, onion-id and listen-port been specified?
    if (pdu->content_type == 0 || pdu->onion_id == NULL || pdu->lport == 0)
    {
        free(line);
        return -1;
    }

    // allocate memory for content
    pdu->content = malloc(pdu->content_length + 1);
    contentp = pdu->content; // point to the beginning

    // read content frm file descriptor
    // read x bytes defined by Content-Length
    for (b = 0, contentp = pdu->content; b < pdu->content_length;
         b++, contentp++, len++)
    {
        if ((ret = read(fd, contentp, 1)) == -1 || !ret)
        {
            free(pdu->content);
            return ret;
        }
    }

    *contentp = '\0'; // NULL terminate potential string
    return len; // amount of bytes read as content == content length
}


/**
 *  Crafts a DChat header string.
 *  Crafts a header string according to the given header_id (see: dchat_encoder.h) together
 *  with the header information stored in the PDU structure.
 *  @param pdu       Pointer to a message structure that holds header information like
 *                   Content-Type, Content-Length, ...
 *  @param header_id Defines for which header a string should be crafted (Content-Type, ...)
 *  @return  Pointer to a header string (stored on heap -> must be freed) or NULL on error
 *           This string is not terminated with \\n or \\r\\n respectively
 */
char*
encode_header(dchat_pdu_t* pdu, int header_id)
{
    char* ret = NULL;       // return value
    char* header = NULL;    // header key string
    char* value = NULL;     // header value string
    int len;                // length of string in total
    int mem_free = 0;       // defines if value must be freed

    // which header should be crafted?
    switch (header_id)
    {
        // first search header key, then set header key string
        case HDR_CONTENT_TYPE:
            header = "Content-Type";
            len = strlen(header); // reserve 12 bytes for "Content-Type"

            switch (pdu->content_type)
            {
                // if header-key = Content-Type, search and set header value
                case CT_TXT_PLAIN:
                    value = "text/plain";
                    break;

                case CT_APPL_OCT:
                    value = "application/octet";
                    break;

                case CT_CTRL_DISC:
                    value = "control/discover";
                    break;

                case CT_CTRL_RPLY:
                    value = "control/replay";
                    break;

                default:
                    return NULL; // ERROR
            }

            len += strlen(value); // reserve another x bytes for the Content-Type value
            break;

        case HDR_CONTENT_LENGTH:
            //FIXME: check content-length
            header = "Content-Length";
            len = strlen(header);
            value = malloc(MAX_INT_STR + 1);
            snprintf(value, MAX_INT_STR, "%d", pdu->content_length);
            len += strlen(value);
            mem_free = 1; // value must be freed
            break;

        case HDR_ONION_ID:
            header = "Onion-ID";
            len = strlen(header);
            value = pdu->onion_id;

            if (value == NULL)
            {
                return NULL;
            }

            len += strlen(value);
            break;

        case HDR_LISTEN_PORT:
            //FIXME: check listen port
            header = "Listen-Port";
            len = strlen(header);
            value = malloc(MAX_INT_STR + 1);
            snprintf(value, MAX_INT_STR, "%d", pdu->lport);
            len += strlen(value);
            mem_free = 1; // value must be freed
            break;

        case HDR_NICKNAME:
            header = "Nickname";
            len = strlen(header);
            value = pdu->nickname;

            if (value == NULL)
            {
                return NULL;
            }

            len += strlen(value);
            break;

        default:
            return NULL; // ERROR
    }

    len += 4; // add three bytes for ':', a " ", '\n' and '\0';

    // allocate memory for header string
    if ((ret = malloc(len)) == NULL)
    {
        return NULL;
    }

    // assemble header string
    ret[0] = '\0';
    strncat(ret, header, strlen(header));
    strncat(ret, ":", 1); // seperate key from value -> "key:value"
    strncat(ret, " ", 1); // add a " " after the semicolon -> "key: value"
    strncat(ret, value, strlen(value)); // add value
    strncat(ret, "\n", 1);
    ret[len - 1] = '\0';

    if (mem_free == 1) // if value has been allocated dynamically
    {
        free(value);
    }

    // return header string
    return ret;
}


/**
 *  Writes a \\r\\n terminated line to a file descriptor.
 *  Writes the given buffer and additionally \\r\\n to the file descriptor.
 *  @param fd  File descriptor where the data will be written to
 *  @param buf Pointer to a buffer holding the data that shall be written.
 *              The buffer must be \0 terminated!
 *  @return Amount of bytes that have been written (inkl. appended \\r\\n)
 */
int
write_line(int fd, char* buf)
{
    char* line;
    char* ret;
    int wr_len;

    // allocate memory for line to write
    if ((line = malloc(strlen(buf) + 3)) == NULL) // +2 for \r\n and \0
    {
        return -1;
    }

    // copy buf
    ret = strncpy(line, buf, strlen(buf));
    line[strlen(buf)] = '\0';

    if (ret != line)
    {
        free(line);
        return -1;
    }

    // append \r\n
    ret = strncat(line, "\r\n", 2);

    if (ret != line)
    {
        free(line);
        return -1;
    }

    // write line
    if ((wr_len = write(fd, line, strlen(line))) == -1)
    {
        free(line);
        return -1;
    }

    free(line);
    return wr_len;
}


/**
 * Converts a PDU to a string that will be written to a file descriptor.
 * Converts the given PDU to string which then will be written to the given file descriptor.
 * First the headers of the PDU will be written, then an empty line and at last the content.
 * (See specification of the dchat protocol)
 * @param fd  File descriptor where the dchat PDU will be written to
 * @param pdu Pointer to a PDU structure holding the header and content data
 * @return Amount of bytes of content that that have been written. This should be equal
 *         to the value defined in the attribute "content_length" of the given PDU structure
 *         or -1 in case of error
 */
int
write_pdu(int fd, dchat_pdu_t* pdu)
{
    char* version = "DCHAT: 1.0\n";  //Version of DCHAT
    char* content_type;              //Content-Type header string
    char* content_length;            //Conent-Length header string
    char* onion_id;                  //Onion address  header string
    char* lport;                     //Listening-port header string
    char* nickname = NULL;           //Nickname string
    char* pdu_raw;                   //Final PDU
    int ret;                         //Return value
    int pdulen=0;                    //Total length of PDU

    ///get Content-Type string
    if ((content_type = encode_header(pdu, HDR_CONTENT_TYPE)) == NULL)
    {
        return -1;
    }
    //get Content-Length string
    else if ((content_length = encode_header(pdu, HDR_CONTENT_LENGTH)) == NULL)
    {
        free(content_type);
        return -1;
    }
    //get Listen-Port string
    else if ((lport = encode_header(pdu, HDR_LISTEN_PORT)) == NULL)
    {
        free(content_type);
        free(content_length);
        return -1;
    }
    //get Onion-ID string
    else if ((onion_id = encode_header(pdu, HDR_ONION_ID)) == NULL)
    {
        free(content_type);
        free(content_length);
        free(lport);
        return -1;
    }
    //nickname is optional
    else if (pdu->nickname != NULL)
    {
        //get Nickname string
        if ((nickname = encode_header(pdu, HDR_NICKNAME)) == NULL)
        {
            free(content_type);
            free(content_length);
            free(lport);
            free(onion_id);
            return -1;
        }
    }

    pdulen  = strlen(version);
    pdulen += strlen(content_type);
    pdulen += strlen(content_length);
    pdulen += strlen(onion_id);
    pdulen += strlen(lport);

    if (nickname != NULL)
    {
        pdulen += strlen(nickname);
    }

    pdulen += 1; //for empty line
    pdulen += pdu->content_length;
    //allocate memory for pdu
    pdu_raw = malloc(pdulen + 1);

    if (pdu_raw == NULL)
    {
        log_msg(LOG_ERR, "write_pdu() failed - could not allocate memory");
        return -1;
    }

    //craft pdu
    pdu_raw[0] = '\0';
    strncat(pdu_raw, version, strlen(version));
    strncat(pdu_raw, content_type, strlen(content_type));
    strncat(pdu_raw, content_length, strlen(content_length));
    strncat(pdu_raw, onion_id, strlen(onion_id));
    strncat(pdu_raw, lport, strlen(lport));

    if (nickname != NULL)
    {
        strncat(pdu_raw, nickname, strlen(nickname));
    }

    strncat(pdu_raw, "\n", 1);
    strncat(pdu_raw, pdu->content, pdu->content_length);
    //write pdu to file descriptor
    ret = write(fd, pdu_raw, pdulen);
    free(content_type);
    free(content_length);
    free(onion_id);
    free(lport);
    free(pdu_raw);
    return ret;
}


/**
 * Initializes a DChat PDU with the given values.
 * @param pdu          Pointer to PDU structure whose members will be initialized
 * @param content_type Content-Type
 * @param onion_id     Onion-ID
 * @param lport        Listening port
 * @param nickname     Nickname
 */
int
init_dchat_pdu(dchat_pdu_t* pdu, int content_type, char* onion_id, int lport, char* nickname)
{
    if(!is_valid_content_type(content_type))
    {
        log_msg(LOG_WARN, "Invalid Content-Type '0x%02x'!", content_type);
        return -1;
    }
    if(!is_valid_onion(onion_id))
    {
        log_msg(LOG_WARN, "Invalid Onion-ID '%s'!", onion_id);
        return -1;
    }
    if(!is_valid_port(lport))
    {
        log_msg(LOG_WARN, "Invalid Listening-Port '%d'!", lport);
        return -1;
    }
    if(!is_valid_nickname(nickname))
    {
        log_msg(LOG_WARN, "Invalid Nickname '%s'!", nickname);
        return -1;
    }

    memset(pdu, 0, sizeof(*pdu));
    pdu->content_type = content_type;
    pdu->onion_id[0]  = '\0';
    strncpy(pdu->onion_id, onion_id, ONION_ADDRLEN);
    pdu->lport        = lport;
    pdu->nickname[0]  = '\0';
    strncpy(pdu->nickname, nickname, MAX_NICKNAME);

    return 0;
}


/**
 * Initializes the content of a DChat PDU with the given value.
 * @param pdu     Pointer to PDU whose content will be initialized
 * @param content Pointer to content
 * @param         len Lenght of content
 */
void
init_dchat_pdu_content(dchat_pdu_t* pdu, char* content, int len)
{
    if((pdu->content = malloc(len)) == NULL)
    {
        fatal("Memory allocation for PDU content failed!");
    }

    memcpy(pdu->content, content, len);
    pdu->content_length = len;
}


/**
 * Checks if the given Content-Type number is a valid DChat Content-Type.
 * @return 1 if valid, 0 otherwise.
 */
int
is_valid_content_type(int content_type)
{
    if(content_type & CT_ALL_MASK)
    {
        return 1;
    }
    
    return 0;
}


/**
 * Checks if the given nickname is a valid DChat nickname.
 * @return 1 if valid, 0 otherwise.
 */
int
is_valid_nickname(char* nickname)
{
    if(nickname == NULL)
    {
      return 0;
    }
    if(strlen(nickname) > MAX_NICKNAME)
    {
      return 0;
    }
    
    //FIXME: check non printable characters
    return 1;
}


/**
 *  Frees all resources dynamically allocated for a PDU structure.
 *  This function frees the allocated memory for the content. In the
 *  future other fields may be freed.
 *  @param pdu Pointer to a PDU structure 
 */
void
free_pdu(dchat_pdu_t* pdu)
{
    if (pdu != NULL)
    {
        if (pdu->content != NULL)
        {
            free(pdu->content);
        }
    }
}


/**
 *  Extracts a fraction of the content.
 *  Part of the content of a PDU will be extracted beginning at offset and
 *  ending at the given terminating character term. The partial content, given
 *  as double pointer should be freed after the successfull call of this function.
 *  @param pdu      Pointer to a pdu containing the content
 *  @param offset   Offset where the extraction will begin
 *  @param term     Terminating character where the extraction will end
 *  @param content  Extracted content
 *  @return Returns offset where the terminating character has
 *          been found in the content of the pdu, -1 on error
 */
int
get_content_part(dchat_pdu_t* pdu, int offset, char term, char** content)
{
    int line_end; // detected end of line, represented as index
    char* ptr;    // content pointer

    // check if offset is within the content
    if (offset >= pdu->content_length)
    {
        return -1;
    }

    // determine line end -> \n or end of content
    for (ptr = (pdu->content + offset), line_end = offset; *ptr != term &&
         line_end < pdu->content_length; ptr++, line_end++);

    // if end of content is reached before \n
    if (line_end == pdu->content_length && *(ptr - 1) != term)
    {
        log_msg(LOG_ERR, "get_content_part() - Could not parse line");
        return -1;
    }

    // reserve enough space for line + \0
    *content = malloc(line_end + 2); // +1 since its an index and +1 for \0

    if (*content == NULL)
    {
        log_msg(LOG_ERR, "get_content_part() - Could not allocate memory");
        return -1;
    }

    // copy data into line buffer
    strncpy(*content, &pdu->content[offset], (line_end - offset + 1));
    (*content)[line_end + 1] = '\0';
    return line_end;
}
