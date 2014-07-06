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
    char* key;          // header key (e.g. Content-Type)
    char* value;        // header value (e.g. text/plain)
    char* delim = ":";  // delimiter char
    char* save_ptr;     // used for strtok
    int len;            // length of value
    int end;            // index of termination chars (\r)\n of value
    int ret;            // return of parsed value
    dchat_v1_t proto;   // DChat V1 headers

    // split line: header format -> key:value
    if ((key = strtok_r(line, delim, &save_ptr)) == NULL)
    {
        return -1;
    }
    else if ((value = strtok_r(NULL, delim, &save_ptr)) == NULL)
    {
        return -1;
    }

    // first character must be a whitespace
    if (strncmp(value, " ", 1) != 0)
    {
        return -1;
    }

    // skip " "
    value++;

    if ((ret = is_valid_termination(value)) == -1)
    {
        return -1;
    }

    // remove termination characters
    value[ret] = '\0';

    if (init_dchat_v1(&proto) == -1)
    {
        return -1;
    }

    // iterate through headers and check if value is valid
    // if valid parse value and set attributes in the PDU
    for (int i = 0; i < HDR_AMOUNT; i++)
    {
        if (!strcmp(key, proto.header[i].header_name))
        {
            return proto.header[i].str_to_pdu(value, pdu);
        }
    }

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

    if (decode_header(pdu, line) == -1 || pdu->version != DCHAT_V1)
    {
        free(line);
        return -1;
    }

    len += strlen(line);
    free(line);

    // read header lines from file descriptors, until
    // an empty line is received
    while ((ret = read_line(fd, &line)) != -1 || ret == 0)
    {
        len += strlen(line);

        // decode read line as header
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

    if (ret <= 0)
    {
        free(line);
        return ret;
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
    dchat_v1_t proto;    // DChat V1 headers
    char* header = NULL; // header key string
    char* value  = NULL; // header value string
    char* line   = NULL; // concetenated header, value pair
    int len;             // length of string in total
    int ret;

    if (init_dchat_v1(&proto) == -1)
    {
        return NULL;
    }

    // iterate through supported headers
    for (int i = 0; i < HDR_AMOUNT; i++)
    {
        if (proto.header[i].header_id == header_id)
        {
            header = proto.header[i].header_name;
            len = strlen(header);
            ret = proto.header[i].pdu_to_str(pdu, &value);

            if (ret == -1)
            {
                return NULL;
            }

            len += strlen(value);
            break;
        }
    }

    len += 4; // add three bytes for ':', a " ", '\n' and '\0';

    // allocate memory for header string
    if ((line = malloc(len)) == NULL)
    {
        fatal("Memory allocation for header-value string failed!");
    }

    // assemble header string
    line[0] = '\0';
    strncat(line, header, strlen(header));
    strncat(line, ":", 1); // seperate key from value -> "key:value"
    strncat(line, " ", 1); // add a " " after the semicolon -> "key: value"
    strncat(line, value, strlen(value)); // add value
    strncat(line, "\n", 1);

    if (ret == 1) // if value has been allocated dynamically
    {
        free(value);
    }

    // return header string
    return line;
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
    char* version;                   //Version of DCHAT
    char* content_type;              //Content-Type header string
    char* content_length;            //Conent-Length header string
    char* onion_id;                  //Onion address  header string
    char* lport;                     //Listening-port header string
    char* nickname = NULL;           //Nickname string
    char* pdu_raw;                   //Final PDU
    int ret;                         //Return value
    int pdulen=0;                    //Total length of PDU

    //get version string
    if ((version = encode_header(pdu, HDR_ID_VER)) == NULL)
    {
        return -1;
    }
    ///get Content-Type string
    else if ((content_type = encode_header(pdu, HDR_ID_CTT)) == NULL)
    {
        free(version);
        return -1;
    }
    //get Content-Length string
    else if ((content_length = encode_header(pdu, HDR_ID_CTL)) == NULL)
    {
        free(version);
        free(content_type);
        return -1;
    }
    //get Listen-Port string
    else if ((lport = encode_header(pdu, HDR_ID_LNP)) == NULL)
    {
        free(version);
        free(content_type);
        free(content_length);
        return -1;
    }
    //get Onion-ID string
    else if ((onion_id = encode_header(pdu, HDR_ID_ONI)) == NULL)
    {
        free(version);
        free(content_type);
        free(content_length);
        free(lport);
        return -1;
    }

    //get Nickname string
    if ((nickname = encode_header(pdu, HDR_ID_NIC)) == NULL)
    {
        free(version);
        free(content_type);
        free(content_length);
        free(lport);
        free(onion_id);
        return -1;
    }

    pdulen  = strlen(version);
    pdulen += strlen(content_type);
    pdulen += strlen(content_length);
    pdulen += strlen(onion_id);
    pdulen += strlen(lport);
    pdulen += strlen(nickname);
    pdulen += 1; //for empty line
    pdulen += pdu->content_length;
    //allocate memory for pdu
    pdu_raw = malloc(pdulen + 1);

    if (pdu_raw == NULL)
    {
        fatal("Memory allocation for PDU failed!");
    }

    //craft pdu
    pdu_raw[0] = '\0';
    strncat(pdu_raw, version, strlen(version));
    strncat(pdu_raw, content_type, strlen(content_type));
    strncat(pdu_raw, content_length, strlen(content_length));
    strncat(pdu_raw, onion_id, strlen(onion_id));
    strncat(pdu_raw, lport, strlen(lport));
    strncat(pdu_raw, nickname, strlen(nickname));
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
 * Converts the version field in the PDU to a string and sets the address of the given
 * value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 0 if value is a valid version, -1 otherwise
 */
int
ver_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    char* version = "1.0";

    if (pdu->version == DCHAT_V1)
    {
        *value = version;
        return 0;
    }

    return -1;
}


/**
 * Converts the content-type field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 if value is a valid content-type (needs to be freed), -1 otherwise
 */
int
ctt_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    dchat_content_types_t content_types;
    char* type;

    if (init_dchat_content_types(&content_types) == -1)
    {
        return -1;
    }

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
            return 1;
        }
    }

    return -1;
}


/**
 * Converts the content-length field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 if value is a valid content-length (needs to be freed), -1 otherwise
 */
int
ctl_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
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
    return 1;
}


/**
 * Converts the onion-id field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 0 if value is a valid onion-id, -1 otherwise
 */
int
oni_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    if (!is_valid_onion(pdu->onion_id))
    {
        return -1;
    }

    *value = pdu->onion_id;
    return 0;
}


/**
 * Converts the listening-port field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 1 if value is a valid listening-port (needs to be freed), -1 otherwise
 */
int
lnp_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
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
    return 1;
}


/**
 * Converts the nickname field in the PDU to a string and sets the
 * address of the given value parameter to this string.
 * @param pdu Pointer to PDU structure
 * @param value Double pointer to string
 * @return 0 if value is a valid nickname, -1 otherwise
 */
int
nic_pdu_to_str(dchat_pdu_t* pdu, char** value)
{
    if (!is_valid_nickname(pdu->nickname))
    {
        return -1;
    }

    *value = pdu->nickname;
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
        HEADER(HDR_ID_VER, HDR_NAME_VER, ver_str_to_pdu, ver_pdu_to_str),
        HEADER(HDR_ID_CTT, HDR_NAME_CTT, ctt_str_to_pdu, ctt_pdu_to_str),
        HEADER(HDR_ID_CTL, HDR_NAME_CTL, ctl_str_to_pdu, ctl_pdu_to_str),
        HEADER(HDR_ID_ONI, HDR_NAME_ONI, oni_str_to_pdu, oni_pdu_to_str),
        HEADER(HDR_ID_LNP, HDR_NAME_LNP, lnp_str_to_pdu, lnp_pdu_to_str),
        HEADER(HDR_ID_NIC, HDR_NAME_NIC, nic_str_to_pdu, nic_pdu_to_str)
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
    pdu->version      = version;
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
