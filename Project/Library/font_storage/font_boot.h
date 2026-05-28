#ifndef FONT_BOOT_H
#define FONT_BOOT_H

#include <stdint.h>

typedef enum
{
    FONT_BOOT_READY = 0,
    FONT_BOOT_IMPORTED,
    FONT_BOOT_NO_SD,
    FONT_BOOT_MOUNT_FAILED,
    FONT_BOOT_IMPORT_FAILED
} FontBootResult;

typedef void (*FontBootStatusCallback)(const char *stage, uint8_t percent, void *user);

FontBootResult FontBoot_EnsureReady(uint8_t *sd_mounted_out);
FontBootResult FontBoot_EnsureReadyEx(uint8_t *sd_mounted_out, FontBootStatusCallback cb, void *user);
const char *FontBoot_ResultToString(FontBootResult result);

#endif
