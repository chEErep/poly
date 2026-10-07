#ifndef LAB_SYSTEM_INFO_H
#define LAB_SYSTEM_INFO_H

#include <stddef.h>
#include <stdint.h>

// Заполнить output ответом вида "ключ=значение\n" по номеру запроса
// Возвращает 1 при успешном формировании текста, 0 при ошибке буфера
// is_error отдельно сообщает, что сформирован ответ status=error
int lab_build_system_response(uint16_t request_type,
                              const uint8_t *request_payload,
                              size_t request_payload_len,
                              char *output,
                              size_t output_size,
                              int *is_error);

#endif
