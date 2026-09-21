#ifndef LAB_PROTOCOL_H
#define LAB_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define LAB_PROTOCOL_VERSION 1u
#define LAB_HEADER_SIZE 24u
#define LAB_MAX_PAYLOAD (64u * 1024u)
#define LAB_DEFAULT_PORT "27015"

enum lab_message_kind {
    LAB_KIND_REQUEST = 1,
    LAB_KIND_RESPONSE = 2,
    LAB_KIND_ERROR = 3
};

enum lab_request_type {
    LAB_GET_OS = 1,
    LAB_GET_TIME = 2,
    LAB_GET_UPTIME = 3,
    LAB_GET_MEMORY = 4
};

typedef struct lab_header {
    uint8_t version;
    uint8_t kind;
    uint16_t type;
    uint32_t request_id;
    uint32_t flags;
    uint32_t payload_len;
    uint32_t reserved;
} lab_header;

void lab_encode_header(uint8_t output[LAB_HEADER_SIZE], const lab_header *header);
int lab_decode_header(lab_header *header, const uint8_t input[LAB_HEADER_SIZE]);
const char *lab_request_name(uint16_t type);

#endif
