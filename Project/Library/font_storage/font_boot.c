#include "font_boot.h"

#include "font_storage.h"
#include "../EN25Q128/EN25Q128.h"
#include "../../FATFS/App/fatfs.h"
#include "ff.h"

#define FONT_BOOT_SD_PATH "0:/Font/HZK16"

typedef struct
{
    FontBootStatusCallback cb;
    void *user;
} FontBootProgressContext;

static void FontBoot_Report(FontBootStatusCallback cb, void *user, const char *stage, uint8_t percent)
{
    if (cb != 0)
    {
        cb(stage, percent, user);
    }
}

static void FontBoot_ImportProgress(uint32_t done, uint32_t total, void *user)
{
    FontBootProgressContext *ctx = (FontBootProgressContext *)user;
    uint8_t percent = 0U;

    if (ctx == 0 || ctx->cb == 0)
    {
        return;
    }

    if (total != 0U)
    {
        percent = (uint8_t)((done * 100UL) / total);
    }

    ctx->cb("Importing...", percent, ctx->user);
}

FontBootResult FontBoot_EnsureReadyEx(uint8_t *sd_mounted_out, FontBootStatusCallback cb, void *user)
{
    FRESULT fres;
    FontStorageResult import_res;
    FontBootProgressContext progress_ctx;

    if (sd_mounted_out != 0)
    {
        *sd_mounted_out = 0U;
    }

    progress_ctx.cb = cb;
    progress_ctx.user = user;

    FontBoot_Report(cb, user, "Init Flash...", 0U);
    EN25QXX_Init();

    FontBoot_Report(cb, user, "Check Font...", 0U);
    FontStorage_Init();
    if (FontStorage_IsReady() != 0U)
    {
        FontBoot_Report(cb, user, "Font Ready", 100U);
        return FONT_BOOT_READY;
    }

    FontBoot_Report(cb, user, "Mount SD...", 0U);
    fres = f_mount(&SDFatFS, SDPath, 1);
    if (fres != FR_OK)
    {
        FontBoot_Report(cb, user, "Mount SD Failed", 0U);
        return FONT_BOOT_MOUNT_FAILED;
    }

    if (sd_mounted_out != 0)
    {
        *sd_mounted_out = 1U;
    }

    FontBoot_Report(cb, user, "Importing...", 0U);
    import_res = FontStorage_ImportFromSDEx(FONT_BOOT_SD_PATH, FontBoot_ImportProgress, &progress_ctx);
    if (import_res != FONT_STORAGE_OK)
    {
        FontBoot_Report(cb, user, "Import Failed", 0U);
        return FONT_BOOT_IMPORT_FAILED;
    }

    FontBoot_Report(cb, user, "Verify Font...", 100U);
    FontStorage_Init();
    if (FontStorage_IsReady() == 0U)
    {
        FontBoot_Report(cb, user, "Verify Failed", 0U);
        return FONT_BOOT_IMPORT_FAILED;
    }

    FontBoot_Report(cb, user, "Font Ready", 100U);
    return FONT_BOOT_IMPORTED;
}

FontBootResult FontBoot_EnsureReady(uint8_t *sd_mounted_out)
{
    return FontBoot_EnsureReadyEx(sd_mounted_out, 0, 0);
}

const char *FontBoot_ResultToString(FontBootResult result)
{
    switch (result)
    {
        case FONT_BOOT_READY:
            return "font ready in external flash";
        case FONT_BOOT_IMPORTED:
            return "font imported from SD to external flash";
        case FONT_BOOT_NO_SD:
            return "sd card not available";
        case FONT_BOOT_MOUNT_FAILED:
            return "sd mount failed";
        case FONT_BOOT_IMPORT_FAILED:
        default:
            return "font import failed";
    }
}
