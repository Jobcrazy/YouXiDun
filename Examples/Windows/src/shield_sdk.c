#include "shield_sdk.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int (__cdecl *ShieldInitFn)(const char *host, const char *app_id);

static HMODULE shield_module;

int shield_sdk_init(const char *dll_path, const char *app_id,
                    char *error_buffer, size_t error_buffer_size) {
    if (!shield_module) {
        shield_module = LoadLibraryA(dll_path);
        if (!shield_module) {
            if (error_buffer && error_buffer_size) {
                snprintf(error_buffer, error_buffer_size,
                         "Unable to load Shield.dll (Windows error %lu)",
                         (unsigned long)GetLastError());
            }
            return -1;
        }
    }

    FARPROC procedure = GetProcAddress(shield_module, "Init");
    if (!procedure) {
        if (error_buffer && error_buffer_size) {
            snprintf(error_buffer, error_buffer_size,
                     "Shield.dll does not export Init (Windows error %lu)",
                     (unsigned long)GetLastError());
        }
        return -1;
    }
    ShieldInitFn init = NULL;
    memcpy(&init, &procedure, sizeof(init));
    return init(NULL, app_id);
}
