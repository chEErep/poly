#include <stdio.h>
#include <string.h>

#include "protocol.h"

static int test_round_trip(void)
{
    lab_header original;
    lab_header decoded;
    uint8_t bytes[LAB_HEADER_SIZE];

    original.version = LAB_PROTOCOL_VERSION;
    original.kind = LAB_KIND_REQUEST;
    original.type = LAB_GET_MEMORY;
    original.request_id = 0x12345678u;
    original.flags = 0x01020304u;
    original.payload_len = 4096u;
    original.reserved = 0;

    lab_encode_header(bytes, &original);
    if (!lab_decode_header(&decoded, bytes)) {
        return 0;
    }

    return original.version == decoded.version
        && original.kind == decoded.kind
        && original.type == decoded.type
        && original.request_id == decoded.request_id
        && original.flags == decoded.flags
        && original.payload_len == decoded.payload_len
        && original.reserved == decoded.reserved;
}

static int test_bad_magic(void)
{
    lab_header header;
    uint8_t bytes[LAB_HEADER_SIZE] = {0};

    return !lab_decode_header(&header, bytes);
}

static int test_network_order(void)
{
    lab_header header = {
        LAB_PROTOCOL_VERSION,
        LAB_KIND_REQUEST,
        0x1122u,
        0x33445566u,
        0x778899AAu,
        0x01020304u,
        0
    };
    uint8_t bytes[LAB_HEADER_SIZE];

    lab_encode_header(bytes, &header);
    return bytes[6] == 0x11 && bytes[7] == 0x22
        && bytes[8] == 0x33 && bytes[9] == 0x44
        && bytes[10] == 0x55 && bytes[11] == 0x66
        && bytes[16] == 0x01 && bytes[17] == 0x02
        && bytes[18] == 0x03 && bytes[19] == 0x04;
}

int main(void)
{
    if (!test_round_trip()) {
        fprintf(stderr, "test_round_trip failed\n");
        return 1;
    }
    if (!test_bad_magic()) {
        fprintf(stderr, "test_bad_magic failed\n");
        return 1;
    }
    if (!test_network_order()) {
        fprintf(stderr, "test_network_order failed\n");
        return 1;
    }

    printf("All protocol tests passed.\n");
    return 0;
}
