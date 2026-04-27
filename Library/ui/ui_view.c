#include "ui_view.h"

#include <stdio.h>
#include <string.h>

#include "../Picture/picture_display.h"
#include "../tftlcd/tftlcd.h"
#include "ui_common.h"

/**
 * @brief 判断扩展名是否属于当前支持的运行时图片格式
 * @param ext 文件扩展名，不包含点号
 * @return uint8_t 1=按图片处理，0=按普通文件处理
 * @details
 * 当前运行时图片分支只支持：
 * 1. bmp
 * 2. jpg
 * 3. jpeg
 * 判断时忽略大小写，兼容 FATFS 返回的大写或小写扩展名。
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
    UINT read_len = 0;
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

    /* 第 1 步：校验索引范围，避免访问文件列表缓存越界。 */
    if (index >= g_file_count)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)"Invalid file index");
        return;
    }

    /* 第 2 步：先显示文件名和扩展名，便于用户确认当前打开的是哪一个文件。 */
    snprintf(line, sizeof(line), "Name: %s", g_file_entries[index].name);
    FRONT_COLOR = BLACK;
    LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)line);
    snprintf(line, sizeof(line), "Ext: %s  Size:%luB",
             g_file_entries[index].ext,
             (unsigned long)g_file_entries[index].size);
    LCD_ShowString(8, 62, 304, 16, 16, (uint8_t *)line);

    /* 第 3 步：使用当前浏览器目录拼出完整 FATFS 路径。
     * 这样无论文件来自根目录还是子目录，查看页都能回到正确的来源目录。 */
    if (!UI_BuildSelectedFilePath(index, path, sizeof(path)))
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Build path failed");
        return;
    }

    /* 第 4 步：如果当前文件是图片，则直接走图片预览分支。
     * 这里统一交给 Picture_Show()，由图片模块内部再根据扩展名分发到 BMP 或 JPG 解码路径。 */
    if (UI_View_IsPictureFile(g_file_entries[index].ext))
    {
        PictureResult picture_ret;

        FRONT_COLOR = BLUE;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Image Preview:");
        LCD_DrawLine(0, 102, 319, 102);

        /* 第 5 步：把图片预览区域固定放在 y=104 之后，
         * 避开标题、文件名和扩展名所在的头部区域。 */
        picture_ret = Picture_Show(path, 0, 104);

        /* 第 6 步：在底部状态栏显示图片解码/显示结果。
         * 只放一条短状态文本，避免遮挡主体图像区域。 */
        LCD_Fill(0, 446, 319, 479, WHITE);
        FRONT_COLOR = (picture_ret == PICTURE_OK) ? GREEN : RED;
        LCD_DrawLine(0, 446, 319, 446);
        LCD_ShowString(8, 452, 304, 16, 16, (uint8_t *)Picture_ResultString(picture_ret));
        FRONT_COLOR = BLUE;
        LCD_ShowString(8, 462, 304, 16, 16, (uint8_t *)"KEY0/KEY2: Back List");
        return;
    }

    /* 第 7 步：非图片文件继续沿用原来的文本/十六进制预览路径。 */
    res = f_open(&file, path, FA_READ);
    if (res != FR_OK)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Open file failed");
        return;
    }

    /* 第 8 步：只读取文件头部一段固定长度样本，用于快速预览。 */
    memset(buf, 0, sizeof(buf));
    res = f_read(&file, buf, FILE_VIEW_READ_MAX, &read_len);
    (void)f_close(&file);
    if (res != FR_OK)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Read file failed");
        return;
    }

    /* 第 9 步：先粗略判断这段样本是否更像普通文本。 */
    for (i = 0; i < read_len; i++)
    {
        uint8_t ch = buf[i];
        if (ch == 0U || (ch < 0x20U && ch != '\r' && ch != '\n' && ch != '\t'))
        {
            likely_text = 0U;
            break;
        }
    }

    /* 第 10 步：对文本样本再做 UTF-8 有效性判断。 */
    is_utf8 = UI_IsValidUtf8(buf, (uint16_t)read_len);
    if (likely_text && !is_utf8)
    {
        /* 第 11 步：普通 ASCII/GB2312 文本可以直接混排显示。 */
        buf[read_len] = '\0';
        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Content (GB2312/ASCII):");
        FRONT_COLOR = BLACK;
        LCD_ShowTextMixed(8, 104, 304, 188, buf);
    }
    else if (likely_text && is_utf8)
    {
        /* 第 12 步：UTF-8 文本先转成当前 LCD 文字链路可处理的 GB2312。 */
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
        uint16_t y = 104;
        uint16_t pos = 0;

        /* 第 13 步：非文本文件继续显示十六进制摘要，方便调试任意二进制文件。 */
        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16,
                       (uint8_t *)(is_utf8 ? "UTF-8 content (hex view):" : "Content (hex, first bytes):"));
        FRONT_COLOR = BLACK;
        while (pos < read_len && y <= 284)
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

            for (j = 0; j < n; j++)
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

    /* 第 14 步：普通文件查看页底部保留返回提示和自动回主页提示。 */
    FRONT_COLOR = BLUE;
    LCD_DrawLine(0, 300, 319, 300);
    LCD_ShowString(8, 308, 304, 16, 16, (uint8_t *)"KEY0/KEY2: Back List");
    LCD_ShowString(8, 326, 304, 16, 16, (uint8_t *)"Auto return main in 30s");
}

void UI_View_OpenSelected(void)
{
    /* 查看页只允许打开普通文件。
     * 如果当前高亮的是 [..] 或目录，说明应该先由列表页处理目录切换，而不是直接进查看页。 */
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
