#include "protocol.h"

static void write_u16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8);
    output[1] = (uint8_t)value;
}

static void write_u32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

static uint16_t read_u16(const uint8_t *input)
{
    return (uint16_t)(((uint16_t)input[0] << 8) | input[1]);
}

static uint32_t read_u32(const uint8_t *input)
{
    return ((uint32_t)input[0] << 24)
         | ((uint32_t)input[1] << 16)
         | ((uint32_t)input[2] << 8)
         | (uint32_t)input[3];
}

void lab_encode_header(uint8_t output[LAB_HEADER_SIZE], const lab_header *header)
{
    output[0] = 'B';
    output[1] = 'S';
    output[2] = 'I';
    output[3] = '1';
    output[4] = header->version;
    output[5] = header->kind;
    write_u16(output + 6, header->type);
    write_u32(output + 8, header->request_id);
    write_u32(output + 12, header->flags);
    write_u32(output + 16, header->payload_len);
    write_u32(output + 20, header->reserved);
}

int lab_decode_header(lab_header *header, const uint8_t input[LAB_HEADER_SIZE])
{
    if (input[0] != 'B' || input[1] != 'S'
        || input[2] != 'I' || input[3] != '1') {
        return 0;
    }

    header->version = input[4];
    header->kind = input[5];
    header->type = read_u16(input + 6);
    header->request_id = read_u32(input + 8);
    header->flags = read_u32(input + 12);
    header->payload_len = read_u32(input + 16);
    header->reserved = read_u32(input + 20);
    return 1;
}

const char *lab_request_name(uint16_t type)
{
    switch (type) {
    case LAB_GET_OS:
        return "GET_OS";
    case LAB_GET_TIME:
        return "GET_TIME";
    case LAB_GET_UPTIME:
        return "GET_UPTIME";
    case LAB_GET_MEMORY:
        return "GET_MEMORY";
    default:
        return "UNKNOWN";
    }
}
