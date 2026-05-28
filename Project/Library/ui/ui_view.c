#include "ui_view.h"

#include <stdio.h>
#include <string.h>

#include "../Picture/picture_display.h"
#include "../audio/mp3_decode_test.h"
#include "../audio/mp3_probe.h"
#include "../audio/wav_player.h"
#include "../tftlcd/tftlcd.h"
#include "ui_common.h"

#define UI_VIEW_PREVIEW_X 0U
#define UI_VIEW_PREVIEW_Y 104U
#define UI_VIEW_PREVIEW_WIDTH 320U
#define UI_VIEW_PREVIEW_HEIGHT 342U

/**
 * @brief 判断扩展名是否属于当前支持的运行时图片格式
 * @param ext 文件扩展名，不包含点号
 * @return uint8_t 1=按图片处理，0=按普通文件处理
 */
static uint8_t UI_View_IsPictureFile(const char *ext)
{
    if (ext == NULL)
    {
        return 0U;
    }

    if (((ext[0] == 'b' || ext[0] == 'B') &&
         (ext[1] == 'm' || ext[1] == 'M') &&
         (ext[2] == 'p' || ext[2] == 'P') &&
         ext[3] == '\0'))
    {
        return 1U;
    }

    if (((ext[0] == 'j' || ext[0] == 'J') &&
         (ext[1] == 'p' || ext[1] == 'P') &&
         (ext[2] == 'g' || ext[2] == 'G') &&
         ext[3] == '\0'))
    {
        return 1U;
    }

    if (((ext[0] == 'j' || ext[0] == 'J') &&
         (ext[1] == 'p' || ext[1] == 'P') &&
         (ext[2] == 'e' || ext[2] == 'E') &&
         (ext[3] == 'g' || ext[3] == 'G') &&
         ext[4] == '\0'))
    {
        return 1U;
    }

    return 0U;
}

void UI_View_Show(uint16_t index)
{
    FIL file;
    FRESULT res;
    UINT read_len = 0U;
    uint8_t buf[FILE_VIEW_READ_MAX + 1U];
    uint8_t gb_buf[FILE_VIEW_READ_MAX * 2U + 1U];
    uint8_t likely_text = 1U;
    uint8_t is_utf8 = 0U;
    uint16_t i;
    char line[128];
    char path[UI_FILE_PATH_MAX];

    BACK_COLOR = WHITE;
    LCD_Clear(WHITE);
    FRONT_COLOR = BLUE;
    LCD_ShowString(8, 8, 304, 16, 16, (uint8_t *)"File View (KEY2)");
    LCD_DrawLine(0, 28, 319, 28);

    /* 第 1 步：校验列表索引，避免访问文件缓存越界。 */
    if (index >= g_file_count)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)"Invalid file index");
        return;
    }

    /* 第 2 步：显示文件基本信息。 */
    snprintf(line, sizeof(line), "Name: %s", g_file_entries[index].name);
    FRONT_COLOR = BLACK;
    LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)line);

    snprintf(line,
             sizeof(line),
             "Ext: %s  Size:%luB",
             g_file_entries[index].ext,
             (unsigned long)g_file_entries[index].size);
    LCD_ShowString(8, 62, 304, 16, 16, (uint8_t *)line);

    /* 第 3 步：基于当前浏览目录拼完整路径。 */
    if (!UI_BuildSelectedFilePath(index, path, sizeof(path)))
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Build path failed");
        return;
    }

    /* 第 4 步：图片文件走图片预览分支。 */
    if (UI_View_IsPictureFile(g_file_entries[index].ext))
    {
        PictureResult picture_ret;
        char preview_status[64];

        /* 每次预览前先清空图片区，避免上一张图残留。 */
        LCD_Fill(UI_VIEW_PREVIEW_X,
                 UI_VIEW_PREVIEW_Y,
                 (uint16_t)(UI_VIEW_PREVIEW_X + UI_VIEW_PREVIEW_WIDTH - 1U),
                 (uint16_t)(UI_VIEW_PREVIEW_Y + UI_VIEW_PREVIEW_HEIGHT - 1U),
                 WHITE);

        picture_ret = Picture_Show(path,
                                   UI_VIEW_PREVIEW_X,
                                   UI_VIEW_PREVIEW_Y,
                                   UI_VIEW_PREVIEW_WIDTH,
                                   UI_VIEW_PREVIEW_HEIGHT);

        /* 结果直接显示在 Image Preview: 后面。 */
        FRONT_COLOR = (picture_ret == PICTURE_OK) ? GREEN : RED;
        snprintf(preview_status,
                 sizeof(preview_status),
                 "Image Preview: %s",
                 (picture_ret == PICTURE_OK) ? "OK" : Picture_ResultString(picture_ret));
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)preview_status);
        LCD_DrawLine(0, 102, 319, 102);

        /* 底部仅保留返回提示。 */
        LCD_Fill(0, 446, 319, 479, WHITE);
        FRONT_COLOR = BLUE;
        LCD_DrawLine(0, 446, 319, 446);
        LCD_ShowString(8, 462, 304, 16, 16, (uint8_t *)"KEY0/KEY2: Back List");
        return;
    }

    /* 第 5 步：非图片文件按原来的文本/十六进制预览逻辑处理。 */
    if (WavPlayer_IsWavExtension(g_file_entries[index].ext))
    {
        WavInfo wav_info;
        WavPlayerResult wav_ret;

        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"WAV playback...");
        printf("[KEY2] Play WAV: %s\r\n", path);

        wav_ret = WavPlayer_PlayFile(path, &wav_info);

        FRONT_COLOR = (wav_ret == WAV_PLAYER_OK) ? GREEN : RED;
        snprintf(line,
                 sizeof(line),
                 "WAV: %s",
                 WavPlayer_ResultString(wav_ret));
        LCD_ShowString(8, 104, 304, 16, 16, (uint8_t *)line);

        if (wav_ret == WAV_PLAYER_OK || wav_ret == WAV_PLAYER_ERR_UNSUPPORTED)
        {
            FRONT_COLOR = BLACK;
            snprintf(line,
                     sizeof(line),
                     "%luHz %uch %ubit",
                     (unsigned long)wav_info.sample_rate,
                     (unsigned int)wav_info.channels,
                     (unsigned int)wav_info.bits_per_sample);
            LCD_ShowString(8, 124, 304, 16, 16, (uint8_t *)line);
        }

        FRONT_COLOR = BLUE;
        LCD_DrawLine(0, 300, 319, 300);
        LCD_ShowString(8, 308, 304, 16, 16, (uint8_t *)"KEY0/KEY2: Back List");
        return;
    }

    if (Mp3Probe_IsMp3Extension(g_file_entries[index].ext))
    {
        Mp3DecodeTestResult mp3_ret;

        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"MP3 playback...");
        printf("[KEY2] Play MP3: %s\r\n", path);

        mp3_ret = Mp3DecodeTest_File(path);

        FRONT_COLOR = (mp3_ret == MP3_DECODE_TEST_OK) ? GREEN : RED;
        snprintf(line,
                 sizeof(line),
                 "MP3: %s",
                 Mp3DecodeTest_ResultString(mp3_ret));
        LCD_ShowString(8, 104, 304, 16, 16, (uint8_t *)line);

        FRONT_COLOR = BLUE;
        LCD_DrawLine(0, 300, 319, 300);
        LCD_ShowString(8, 308, 304, 16, 16, (uint8_t *)"KEY0/KEY2: Back List");
        return;
    }

    res = f_open(&file, path, FA_READ);
    if (res != FR_OK)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Open file failed");
        return;
    }

    memset(buf, 0, sizeof(buf));
    res = f_read(&file, buf, FILE_VIEW_READ_MAX, &read_len);
    (void)f_close(&file);
    if (res != FR_OK)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Read file failed");
        return;
    }

    for (i = 0U; i < read_len; i++)
    {
        uint8_t ch = buf[i];
        if (ch == 0U || (ch < 0x20U && ch != '\r' && ch != '\n' && ch != '\t'))
        {
            likely_text = 0U;
            break;
        }
    }

    is_utf8 = UI_IsValidUtf8(buf, (uint16_t)read_len);
    if (likely_text && !is_utf8)
    {
        buf[read_len] = '\0';
        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Content (GB2312/ASCII):");
        FRONT_COLOR = BLACK;
        LCD_ShowTextMixed(8, 104, 304, 188, buf);
    }
    else if (likely_text && is_utf8)
    {
        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Content (UTF-8->GB2312):");
        FRONT_COLOR = BLACK;
        if (UI_Utf8ToGb2312(buf, (uint16_t)read_len, gb_buf, (uint16_t)sizeof(gb_buf)))
        {
            LCD_ShowTextMixed(8, 104, 304, 188, gb_buf);
        }
        else
        {
            LCD_ShowString(8, 104, 304, 16, 16, (uint8_t *)"Convert failed");
        }
    }
    else
    {
        uint16_t y = 104U;
        uint16_t pos = 0U;

        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16,
                       (uint8_t *)(is_utf8 ? "UTF-8 content (hex view):" : "Content (hex, first bytes):"));
        FRONT_COLOR = BLACK;
        while (pos < read_len && y <= 284U)
        {
            char hexline[64];
            uint16_t j;
            uint16_t n = (uint16_t)(((read_len - pos) > 8U) ? 8U : (read_len - pos));
            char *cursor = hexline;
            int written = snprintf(cursor,
                                   (size_t)(sizeof(hexline) - (size_t)(cursor - hexline)),
                                   "%04X:",
                                   pos);
            if (written < 0)
            {
                break;
            }
            cursor += written;

            for (j = 0U; j < n; j++)
            {
                written = snprintf(cursor,
                                   (size_t)(sizeof(hexline) - (size_t)(cursor - hexline)),
                                   " %02X",
                                   buf[pos + j]);
                if (written < 0)
                {
                    break;
                }
                cursor += written;
            }

            LCD_ShowString(8, y, 304, 16, 16, (uint8_t *)hexline);
            y = (uint16_t)(y + 18U);
            pos = (uint16_t)(pos + n);
        }
    }

    FRONT_COLOR = BLUE;
    LCD_DrawLine(0, 300, 319, 300);
    LCD_ShowString(8, 308, 304, 16, 16, (uint8_t *)"KEY0/KEY2: Back List");
    LCD_ShowString(8, 326, 304, 16, 16, (uint8_t *)"Auto return main in 30s");
}

void UI_View_OpenSelected(void)
{
    /* 查看页只允许打开普通文件。 */
    if (g_file_count == 0U || g_selected_index >= g_file_count)
    {
        return;
    }
    if (g_file_entries[g_selected_index].type != UI_ENTRY_FILE)
    {
        return;
    }

    g_view_index = g_selected_index;
    UI_EnterPage(UI_PAGE_FILE_VIEW);
    UI_View_Show(g_view_index);
    printf("[KEY2] Open file: %s\r\n", g_file_entries[g_view_index].name);
}
