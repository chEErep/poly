#ifndef LAB_PROTOCOL_H
#define LAB_PROTOCOL_H
#define LAB_MAX_KEY_BLOB 512u
#include <stddef.h>
#include <stdint.h>

// Общие правила обмена: клиент и сервер обязаны понимать заголовок одинаково
#define LAB_PROTOCOL_VERSION 1u
#define LAB_HEADER_SIZE 24u
#define LAB_MAX_PAYLOAD (64u * 1024u)
#define LAB_DEFAULT_PORT "27015"

// kind отвечает на вопрос "это запрос, обычный ответ или сообщение об ошибке?"
enum lab_message_kind {
    LAB_KIND_REQUEST = 1,
    LAB_KIND_RESPONSE = 2,
    LAB_KIND_ERROR = 3
};

// Отдельный номер запроса для каждого уже реализованного вида информации
enum lab_request_type {
    LAB_GET_OS = 1,
    LAB_GET_TIME = 2,
    LAB_GET_UPTIME = 3,
    LAB_GET_MEMORY = 4,
    LAB_GET_DRIVES = 5,
    LAB_GET_DISK_SPACE = 6,
    LAB_GET_OWNER = 7,
    LAB_GET_ACL = 8,
    LAB_KEY_EXCHANGE = 100,
    LAB_SECURE_FRAME = 101
};

// Логическое представление заголовка. В сеть отправляются НЕ байты этой
// структуры, а ровно 24 байта, собранные функцией lab_encode_header()
typedef struct lab_header {
    uint8_t version;
    uint8_t kind;
    uint16_t type;
    uint32_t request_id;
    uint32_t flags;
    uint32_t payload_len;
    uint32_t reserved;
} lab_header;

// encode: поля -> сетевые байты; decode: сетевые байты -> поля
// decode возвращает 0, если первые четыре байта не равны "BSI1"
void lab_encode_header(uint8_t output[LAB_HEADER_SIZE], const lab_header *header);
int lab_decode_header(lab_header *header, const uint8_t input[LAB_HEADER_SIZE]);
const char *lab_request_name(uint16_t type);

#endif
