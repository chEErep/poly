#ifndef LAB_SYSTEM_INFO_H
#define LAB_SYSTEM_INFO_H

#include <stddef.h>
#include <stdint.h>

int lab_build_system_response(uint16_t request_type,
                              char *output,
                              size_t output_size,
                              int *is_error);

#endif
