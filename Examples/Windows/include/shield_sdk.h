#ifndef WINDEMO_SHIELD_SDK_H
#define WINDEMO_SHIELD_SDK_H

#include <stddef.h>

int shield_sdk_init(const char *dll_path, const char *app_id,
                    char *error_buffer, size_t error_buffer_size);

#endif
