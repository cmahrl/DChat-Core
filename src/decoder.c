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


/** @file decoder.c
 *  This file contains core functions to decode and encode DChat Packet Data Units
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "dchat_h/decoder.h"
#include "dchat_h/network.h"
#include "dchat_h/log.h"
#include "dchat_h/util.h"


/**
 *  Decodes a string into a DChat header.
 *  Attempts to decode the given \\n terminated line and sets
 *  corresponding header attributes in the given pdu.
 *  @param pdu  Pointer to PDU structure where header attributes
 *  will be set
 *  @param line Line to parse for dchat-headers; must be \\n terminated
 *  @return 0 if line is a dchat header, -1 otherwise
 */
int
decode_header(dchat_pdu_t* pdu, char* line)
{
    char* temp;
    char* key;          // header key (e.g. Content-Type)
    char* value;        // header value (e.g. text/plain)
    char* delim = ":";  // delimiter char
    char* save_ptr;     // used for strtok
    int len;            // length of value
    int end;            // index of termination chars (\r)\n of value
    int ret;            // return of parsed value
    dchat_v1_t proto;   // DChat V1 headers

    if (line == NULL)
    {
        return -1;
    }

    // copy line to work with
    temp = malloc(strlen(line) + 1);

    if (temp == NULL)
    {
        fatal("Memory allocation for temporary decoder line failed!");
    }

    temp[0] = '\0';
    strcat(temp, line);

    // split line: header format -> key:value
    if ((key = strtok_r(temp, delim, &save_ptr)) == NULL)
    {
        free(temp);
        return -1;
    }
    else if ((value = strtok_r(NULL, delim, &save_ptr)) == NULL)
    {
        free(temp);
        return -1;
    }

    // only split the very first token from temp, to keep
    // the value one token (containing possible delim chars)
    if (save_ptr != NULL)
    {
        value[strlen(value)] = *delim;
    }

    // first character must be a whitespace
    if (strncmp(value, " ", 1) != 0)
    {
        free(temp);
        return -1;
    }

    // skip " "
    value++;

    if ((ret = is_valid_termination(value)) == -1)
    {
        free(temp);
        return -1;
    }

    // remove termination characters
    value[ret] = '\0';

    if (init_dchat_v1(&proto) == -1)
    {
        free(temp);
        return -1;
    }

    // iterate through headers and check if value is valid
    // if valid parse value and set attributes in the PDU
    for (int i = 0; i < HDR_AMOUNT; i++)
    {
        if (!strcmp(key, proto.header[i].header_name))
        {
            free(temp);
            return proto.header[i].str_to_pdu(value, pdu);
        }
    }

    free(temp);
    return -1;
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
    char* ptr;             // line pointer
    char* alc_ptr = NULL;  // used for realloc
    int len = 1;           // current length of string (at least 1 char)
    int ret;               // return value
    *line = NULL;

    do
    {
        // allocate memory for new character
        alc_ptr = realloc(*line, len + 1);

        if (alc_ptr == NULL)
        {
            if (*line != NULL)
            {
                free(*line);
            }

            fatal("Reallocation of input string failed!");
        }

        *line = alc_ptr;
        ptr = *line + len - 1;
        len++;
    }
    while ((ret = read(fd, ptr, 1)) > 0 && *ptr != '\n');

    // on error or EOF of read
    if (ret <= 0)
    {
        free(*line);
        return ret;
    }

    // terminate string
    *(ptr + 1) = '\0';
    return len - 1; // length of string excluding \0
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

    // first header must be version header
    if (decode_header(pdu, line) == -1 || pdu->version != DCHAT_V1)
    {
        ret = -1;
    }

    if (ret != -1)
    {
        len += strlen(line);
        free(line);

        // read header lines from file descriptors, until
        // an empty line is received
        while ((ret = read_line(fd, &line)) != -1 || ret == 0)
        {
            len += strlen(line);

            if (decode_header(pdu, line) == -1)
            {
                // if line is not a header, it must be an empty line
                if (!strcmp(line, "\n") || !strcmp(line, "\r\n"))
                {
                    break; // All headers have been read
                }

                ret = -1;
                break;
            }

            free(line);
        }
    }

    // On error print illegal line
    if (ret == -1)
    {
        log_msg(LOG_ERR, "Illegal PDU header received: '%s'", line);
    }

    // EOF or ERROR
    if (ret <= 0)
    {
        free(line);
        return ret;
    }

    // has content type, onion-id and listen-port been specified?
    if (pdu->content_type == 0 || pdu->onion_id == NULL || pdu->lport == 0)
    {
        log_msg(LOG_ERR, "Mandatory PDU headers are missing!");
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
int
encode_header(dchat_pdu_t* pdu, int header_id, char** headerline)
{
    dchat_v1_t proto;    // DChat V1 headers
    char* header = NULL; // header key string
    char* value  = NULL; // header value string
    int len;             // length of string in total
    int ret;

    if (init_dchat_v1(&proto) == -1)
    {
        return -1;
    }

    // iterate through supported headers
    for (int i = 0; i < HDR_AMOUNT; i++)
    {
        if (proto.header[i].header_id == header_id)
        {
            header = proto.header[i].header_name;
            len = strlen(header);

            if ((ret = proto.header[i].pdu_to_str(pdu, &value)) == -1)
            {
                return -1;
            }

            // check if header is mandatory, if no value has been set
            // in the pdu structure
            if (ret == 1)
            {
                // if header is mandatory -> raise error
                // otherwise just return and do nothing
                if (proto.header[i].mandatory)
                {
                    return -1;
                }

                return 1;
            }

            len += strlen(value);
            len += 4; // add three bytes for ':', a " ", '\n' and '\0';

            // allocate memory for header string
            if ((*headerline = malloc(len)) == NULL)
            {
                fatal("Memory allocation for header-value string failed!");
            }

            // assemble header string
            *headerline[0] = '\0';
            strncat(*headerline, header, strlen(header));
            strncat(*headerline, ":", 1); // seperate key from value -> "key:value"
            strncat(*headerline, " ", 1); // add a " " after the semicolon -> "key: value"
            strncat(*headerline, value, strlen(value)); // add value
            strncat(*headerline, "\n", 1);
            // free converted pdu structure value
            free(value);
            return 0;
        }
    }

    return -1;
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
    dchat_v1_t proto;                //Available DChat headers
    char* header;                    //DChat header
    char* pdu_raw;                   //Final PDU
    int ret;                         //Return value
    int pdulen=0;                    //Total length of PDU

    if (init_dchat_v1(&proto) == -1)
    {
        return -1;
    }

    // get version header string
    if ((ret = encode_header(pdu, HDR_ID_VER, &header)) == -1 || ret == 1)
    {
        return -1;
    }

    pdulen += strlen(header);

    if ((pdu_raw = malloc(pdulen + 1)) == NULL)
    {
        fatal("Memory allocation for pdu failed!");
    }

    // copy version header to raw pdu
    pdu_raw[0] = '\0';
    strcat(pdu_raw, header);
    free(header);

    // iterate through supported headers
    for (int i = 0; i < HDR_AMOUNT; i++)
    {
        // get header strings except version header, if set in pdu structure
        if (proto.header[i].header_id != HDR_ID_VER)
        {
            // get header string
            if ((ret = encode_header(pdu, proto.header[i].header_id, &header)) == -1)
            {
                free(pdu_raw);
                return -1;
            }

            // if header was mandatory, but value was not set in pdu structure
            // raise an error
            if (ret == 1)
            {
                if (proto.header[i].mandatory)
                {
                    free(pdu_raw);
                    return -1;
                }

                continue;
            }

            // (re)allocate memory for new header (excluding \0)
            pdulen += strlen(header);
            pdu_raw = realloc(pdu_raw, pdulen);

            if (pdu_raw == NULL)
            {
                fatal("Reallocation of pdu failed!");
            }

            // copy header to pdu
            strcat(pdu_raw, header);
            free(header);
        }
    }

    // (re)allocate memory for empty line and content (excluding \0)
    pdulen += pdu->content_length + 1;
    pdu_raw = realloc(pdu_raw, pdulen);

    if (pdu_raw == NULL)
    {
        fatal("Reallocation of pdu failed!");
    }

    // add empty line
    strcat(pdu_raw, "\n");
    // add content
    strncat(pdu_raw, pdu->content, pdu->content_length);
    //write pdu to file descriptor
    ret = write(fd, pdu_raw, pdulen);
    free(pdu_raw);
    return pdulen;
}


/**
 * Parses the given value to a supported version of DChat
 * and sets, if valid, its value in the PDU structure.
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid version, -1 otherwise
 */
int
ver_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    char* version = "1.0";

    if (!strcmp(value, version))
    {
        pdu->version = DCHAT_V1;
        return 0;
    }

    return -1;
}


/**
 * Parses the given value to a supported content-type of DChat
 * and sets, if valid, its value in the PDU structure..
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid content-type, -1 otherwise
 */
int
ctt_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    int found = 0;
    dchat_content_types_t ctt;

    if (init_dchat_content_types(&ctt) == -1)
    {
        return -1;
    }

    for (int i = 0; i < CTT_AMOUNT; i++)
    {
        if (!strcmp(value, ctt.type[i].ctt_name))
        {
            pdu->content_type = ctt.type[i].ctt_id;
            found = 1;
        }
    }

    return found ? 0 : -1;
}


/**
 * Parses the given value to a content-length and sets its
 * value, if valid, in the given PDU structure.
 * A valid content-length does not exceed MAX_CONTENT_LEN.
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid content-length, -1 otherwise
 */
int
ctl_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    int length = -1;
    char* ptr;
    // convert string to int
    length = (int) strtol(value, &ptr, 10);

    // check if its a valid content-length
    if (ptr[0] != '\0' || !is_valid_content_length(length))
    {
        return -1;
    }

    pdu->content_length = length;
    return 0;
}


/**
 * Parses the given value to an onion address and sets its value
 * , if valid, in the given PDU structure.
 * A valid onion address is max. 16 characters long (22 characters
 * including the prefix ".onion").
 * @see is_valid_onion
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid onion address, -1 otherwise
 */
int
oni_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    int len = strlen(value);

    if (len != ONION_ADDRLEN || !is_valid_onion(value))
    {
        return -1;
    }

    // copy onion address bytes
    pdu->onion_id[0] = '\0';
    strncat(pdu->onion_id, value, ONION_ADDRLEN);
    return 0;
}


/**
 * Parses the given value to a listening port and sets its value,
 * if valid, in the given PDU structure.
 * @see is_valid_port
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid listening port, -1 otherwise
 */
int
lnp_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    int lport;
    char* ptr;
    // convert string to int
    lport = (int) strtol(value, &ptr, 10);

    // check if it is a valid port
    if (ptr[0] != '\0' || !is_valid_port(lport))
    {
        return -1;
    }

    pdu->lport = lport;
    return 0;
}


/**
 * Parses the given value to a nickname and sets its value,
 * if valid, in the given PDU structure.
 * MAX_NICKNAME characters will be copied to the PDU structure,
 * thus if the given value is longer the rest will be cut off.
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid nickname, -1 otherwise
 */
int
nic_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    int len = strlen(value);
    pdu->nickname[0] = '\0';

    if (len > MAX_NICKNAME)
    {
        len = MAX_NICKNAME;
    }

    // copy whole nickname
    strncat(pdu->nickname, value, len);
    return 0;
}


/**
 * Parses the given value to a struct tm and sets its value,
 * if valid, in the given PDU structure.
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid datetime string, -1 otherwise
 */
int
dat_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    if (strptime(value, "%a, %d %b %Y %H:%M:%S GMT",
                 &pdu->sent) == NULL)
    {
        return -1;
    }

    return 0;
}

/**
 * Parses the given value to a server field and sets its value,
 * if valid, in the given PDU structure.
 * @param value String to parse
 * @param pdu Pointer to PDU structure
 * @return 0 if value is a valid server string, -1 otherwise
 */
int
srv_str_to_pdu(char* value, dchat_pdu_t* pdu)
{
    if ((pdu->server = malloc(strlen(value) + 1)) == NULL)
    {
        fatal("Memory allocation for server failed!");
    }

    pdu->server[0] = '\0';
    strcat(pdu->server, value);
    return 0;
}


/**
 * Converts the version field in the PDU to a string and sets the address of the given
 * value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
ver_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    char* version = "1.0";

    // nothing has been set
    if (pdu->version == 0)
    {
        return 1;
    }

    // if version is V1
    if (pdu->version == DCHAT_V1)
    {
        *value = malloc(strlen(version) + 1);

        if (*value == NULL)
        {
            fatal("Memory allocation for version failed!");
        }

        *value[0] = '\0';
        strcat(*value, version);
        return 0;
    }

    return -1;
}


/**
 * Converts the content-type field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
ctt_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    dchat_content_types_t content_types;
    char* type;

    // content type has not been set
    if (pdu->content_type == 0)
    {
        return 1;
    }

    // init available content types
    if (init_dchat_content_types(&content_types) == -1)
    {
        return -1;
    }

    // iterate through content-types and build a content type string
    for (int i = 0; i < CTT_AMOUNT; i++)
    {
        if (content_types.type[i].ctt_id == pdu->content_type)
        {
            type = content_types.type[i].ctt_name;
            *value = malloc(strlen(type) + 1);

            if (*value == NULL)
            {
                fatal("Memory allocation for content-type failed!");
            }

            *value[0] = '\0';
            strncat(*value, type, strlen(type));
            return 0;
        }
    }

    return -1;
}


/**
 * Converts the content-length field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
ctl_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    // check if content-length is valid
    if (!is_valid_content_length(pdu->content_length))
    {
        return -1;
    }

    *value = malloc(MAX_INT_STR + 1);

    if (*value == NULL)
    {
        fatal("Memory allocation for content-length failed!");
    }

    snprintf(*value, MAX_INT_STR, "%d", pdu->content_length);
    return 0;
}


/**
 * Converts the onion-id field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
oni_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    // no onion-id has been set
    if (pdu->onion_id[0] == '\0')
    {
        return 1;
    }

    // check if set onion id is valid
    if (!is_valid_onion(pdu->onion_id))
    {
        return -1;
    }

    //copy onion id
    *value = malloc(strlen(pdu->onion_id) + 1);

    if (*value == NULL)
    {
        fatal("Memory allocation for onion-id failed!");
    }

    *value[0] = '\0';
    strcat(*value, pdu->onion_id);
    return 0;
}


/**
 * Converts the listening-port field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
lnp_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    // listening port has not been specified
    if (pdu->lport == 0)
    {
        return 1;
    }

    // check if listening port is valid
    if (!is_valid_port(pdu->lport))
    {
        return -1;
    }

    *value = malloc(MAX_INT_STR + 1);

    if (*value == NULL)
    {
        fatal("Memory allocation for listening-port failed!");
    }

    snprintf(*value, MAX_INT_STR, "%d", pdu->lport);
    return 0;
}


/**
 * Converts the nickname field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
nic_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    if (pdu->nickname[0] == '\0')
    {
        return 1;
    }

    if (!is_valid_nickname(pdu->nickname))
    {
        return -1;
    }

    //copy nickname
    *value = malloc(strlen(pdu->nickname) + 1);

    if (*value == NULL)
    {
        fatal("Memory allocation for nickname failed!");
    }

    *value[0] = '\0';
    strcat(*value, pdu->nickname);
    return 0;
}


/**
 * Converts the sent field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
dat_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    int max_len = 100;

    // check if date field is empty
    if (iszero(&pdu->sent, sizeof(pdu->sent)))
    {
        return 1;
    }

    *value = malloc(max_len);

    if (*value == NULL)
    {
        fatal("Memory allocation for date failed!");
    }

    *value[0] = '\0';
    strftime(*value, max_len, "%a, %d %b %Y %H:%M:%S GMT", &pdu->sent);
    return 0;
}


/**
 * Converts the server field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 field was not set in pdu structure, 0 on success (string must be freed),
 * -1 in case of error (e.g. illegal value in pdu structure , ...)
 */
int
srv_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    if (pdu->server == NULL)
    {
        return 1;
    }

    *value = malloc(strlen(pdu->server) + 1);

    if (*value == NULL)
    {
        fatal("Memory allocation for date failed!");
    }

    *value[0] = '\0';
    strcat(*value, pdu->server);
    return 0;
}


/**
 * Initializes a content-types structure with all available
 * content-types in DChat.
 * The structure the given Pointer is pointing to, will be
 * initialized with all content-types supported by DChat (e.g
 * "text/plain", "control/discover", ...). The name of the
 * content-type will be set, as well as its ID.
 * @param ctt Pointer to content-types structure to initialize
 * @return Initialized content-types structure.
 */
int
init_dchat_content_types(dchat_content_types_t* ctt)
{
    int temp_size;
    int ctt_size;
    dchat_content_type_t temp[] =
    {
        CONTENT_TYPE(CTT_ID_TXT, CTT_NAME_TXT),
        CONTENT_TYPE(CTT_ID_BIN, CTT_NAME_BIN),
        CONTENT_TYPE(CTT_ID_DSC, CTT_NAME_DSC),
        CONTENT_TYPE(CTT_ID_RPY, CTT_NAME_RPY),
    };
    temp_size = sizeof(temp) / sizeof(temp[0]);

    if (temp_size > CTT_AMOUNT)
    {
        return -1;
    }

    memset(ctt, 0, sizeof(*ctt));
    memcpy(ctt->type, &temp, sizeof(temp));
    return 0;
}


/**
 * Initializes a DChat V1 structure containing all available and
 * supported headers of the DChat V1 protocol.
 * The given structure will be initialized with all headers
 * available. The header IDs and names will be set and function addresses
 * pointing to corresponding value parsers will be set too.
 * @param proto
 * @return 0 if structure has been initialized successfully, -1 otherwise
 */
int
init_dchat_v1(dchat_v1_t* proto)
{
    int temp_size;
    int proto_size;
    // available headers
    dchat_header_t temp[] =
    {
        HEADER(HDR_ID_VER, HDR_NAME_VER, 1, ver_str_to_pdu, ver_pdu_to_str),
        HEADER(HDR_ID_CTT, HDR_NAME_CTT, 1, ctt_str_to_pdu, ctt_pdu_to_str),
        HEADER(HDR_ID_CTL, HDR_NAME_CTL, 1, ctl_str_to_pdu, ctl_pdu_to_str),
        HEADER(HDR_ID_ONI, HDR_NAME_ONI, 1, oni_str_to_pdu, oni_pdu_to_str),
        HEADER(HDR_ID_LNP, HDR_NAME_LNP, 1, lnp_str_to_pdu, lnp_pdu_to_str),
        HEADER(HDR_ID_NIC, HDR_NAME_NIC, 0, nic_str_to_pdu, nic_pdu_to_str),
        HEADER(HDR_ID_DAT, HDR_NAME_DAT, 0, dat_str_to_pdu, dat_pdu_to_str),
        HEADER(HDR_ID_SRV, HDR_NAME_SRV, 0, srv_str_to_pdu, srv_pdu_to_str)
    };
    temp_size = sizeof(temp) / sizeof(temp[0]);

    if (temp_size > HDR_AMOUNT)
    {
        return -1;
    }

    memset(proto, 0, sizeof(*proto));
    memcpy(proto->header, &temp, sizeof(temp));
    return 0;
}


/**
 * Initializes a DChat PDU with the given values.
 * @param pdu          Pointer to PDU structure whose members will be initialized
 * @param version      Version of DChat Protocol
 * @param content_type Content-Type
 * @param onion_id     Onion-ID
 * @param lport        Listening port
 * @param nickname     Nickname
 */
int
init_dchat_pdu(dchat_pdu_t* pdu, float version, int content_type,
               char* onion_id, int lport,
               char* nickname)
{
    if (!is_valid_version(version))
    {
        log_msg(LOG_WARN, "Invalid version '%2.1f'!", version);
        return -1;
    }

    if (!is_valid_content_type(content_type))
    {
        log_msg(LOG_WARN, "Invalid Content-Type '0x%02x'!", content_type);
        return -1;
    }

    if (!is_valid_onion(onion_id))
    {
        log_msg(LOG_WARN, "Invalid Onion-ID '%s'!", onion_id);
        return -1;
    }

    if (!is_valid_port(lport))
    {
        log_msg(LOG_WARN, "Invalid Listening-Port '%d'!", lport);
        return -1;
    }

    if (!is_valid_nickname(nickname))
    {
        log_msg(LOG_WARN, "Invalid Nickname '%s'!", nickname);
        return -1;
    }

    memset(pdu, 0, sizeof(*pdu));
    // set dchat version
    pdu->version      = version;
    // set content-type
    pdu->content_type = content_type;
    // set hostname
    pdu->onion_id[0]  = '\0';
    strncpy(pdu->onion_id, onion_id, ONION_ADDRLEN);
    // set listening port
    pdu->lport        = lport;
    // set nickname
    pdu->nickname[0]  = '\0';
    strncpy(pdu->nickname, nickname, MAX_NICKNAME);
    // set initialization datetime
    time_t now        = time(0);
    struct tm tm      = *gmtime(&now);
    memcpy(&pdu->sent, &tm, sizeof(struct tm));
    // set servername
    char* package_name = PACKAGE_NAME;
    char* package_version = PACKAGE_VERSION;
    pdu->server = malloc(strlen(package_name)+strlen(package_version)+2);

    if (pdu->server == NULL)
    {
        fatal("Memory allocation for server failed!");
    }

    pdu->server[0] = '\0';
    strcat(pdu->server, package_name);
    strcat(pdu->server, "/");
    strcat(pdu->server, package_version);
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
    if ((pdu->content = malloc(len)) == NULL)
    {
        fatal("Memory allocation for PDU content failed!");
    }

    memcpy(pdu->content, content, len);
    pdu->content_length = len;
}


/**
 * Checks if the given version is a valid and supported DChat version.
 * @return 1 if valid, 0 otherwise
 */
int
is_valid_version(float version)
{
    if (version == DCHAT_V1)
    {
        return 1;
    }

    return 0;
}


/**
 * Checks if the given string is terminated with \n or \r\n respectively.
 * @param value String to check
 * @return Index of termination character, -1 otherwise
 */
int
is_valid_termination(char* value)
{
    int len;
    int end;
    len = strlen(value);

    if (value == NULL)
    {
        return -1;
    }

    // value must end with \n
    if (len < 1 || value[len - 1] != '\n')
    {
        return -1;
    }

    end = len - 1;

    if (end > 1 && value[end - 1] == '\r')
    {
        end--;
    }

    return end;
}


/**
 * Checks if the given Content-Type number is a valid DChat Content-Type.
 * @return 1 if valid, 0 otherwise.
 */
int
is_valid_content_type(int content_type)
{
    if (content_type & CTT_MASK_ALL)
    {
        return 1;
    }

    return 0;
}


/**
 * Checks if the given Content-Length is a valid DChat Content-Length.
 * Content-Length must be between 0 and MAX_CONTENT_LEN.
 * @return 1 if valid, 0 otherwise.
 */
int
is_valid_content_length(int ctl)
{
    if (ctl >= 0 && ctl <= MAX_CONTENT_LEN)
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
    int len;

    if (nickname == NULL)
    {
        return 0;
    }

    len = strlen(nickname);

    if (len == 0 || len > MAX_NICKNAME)
    {
        return 0;
    }

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

        if (pdu->server != NULL)
        {
            free(pdu->server);
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
        log_msg(LOG_ERR, "Could not extract partial content!");
        return -1;
    }

    // determine line end -> \n or end of content
    for (ptr = (pdu->content + offset), line_end = offset; *ptr != term &&
         line_end < pdu->content_length; ptr++, line_end++);

    // if end of content is reached before \n
    if (line_end == pdu->content_length && *(ptr - 1) != term)
    {
        log_msg(LOG_ERR, "Could not extract partial content!");
        return -1;
    }

    // reserve enough space for line + \0
    *content = malloc(line_end + 2); // +1 since its an index and +1 for \0

    if (*content == NULL)
    {
        fatal("Memory allocation for partial content failed!");
    }

    // copy data into line buffer
    strncpy(*content, &pdu->content[offset], (line_end - offset + 1));
    (*content)[line_end + 1] = '\0';
    return line_end;
}
