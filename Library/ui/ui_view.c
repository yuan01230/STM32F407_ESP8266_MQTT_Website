#include "ui_view.h"

#include <stdio.h>
#include <string.h>

#include "../tftlcd/tftlcd.h"
#include "ui_common.h"

/* ============================================================
 * File View Module (ui_view.c)
 * - 负责打开并显示选中文件内容
 * - 负责文本/UTF-8/二进制摘要显示分支
 * ============================================================ */

/* -------------------- static 前置声明区 -------------------- */
/* 本文件无内部 static 函数 */

/**
 * @file ui_view.c
 * @brief 文件查看页面模块
 */

/**
 * @brief 显示指定文件内容
 * @param index 文件索引
 * @retval 无
 * @details
 * 功能：在查看页展示文件头部内容样本。
 * 输入：文件索引、文件缓存信息。
 * 输出：根据内容类型显示文本或十六进制摘要。
 * 分支：
 * 1. GB2312/ASCII 文本：直接显示；
 * 2. UTF-8 文本：先转 GB2312 再显示；
 * 3. 其他内容：按 hex 摘要显示。
 */
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
    char path[80];

    BACK_COLOR = WHITE;
    LCD_Clear(WHITE);
    FRONT_COLOR = BLUE;
    LCD_ShowString(8, 8, 304, 16, 16, (uint8_t *)"File View (KEY2)");
    LCD_DrawLine(0, 28, 319, 28);

    /* 防御：索引越界时直接提示并返回 */
    if (index >= g_file_count)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)"Invalid file index");
        return;
    }

    snprintf(line, sizeof(line), "Name: %s", g_file_entries[index].name);
    FRONT_COLOR = BLACK;
    LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)line);
    snprintf(line, sizeof(line), "Ext: %s  Size:%luB", g_file_entries[index].ext, (unsigned long)g_file_entries[index].size);
    LCD_ShowString(8, 62, 304, 16, 16, (uint8_t *)line);

    snprintf(path, sizeof(path), "0:/%s", g_file_entries[index].name);
    /* 打开并读取文件前 FILE_VIEW_READ_MAX 字节作为显示样本 */
    res = f_open(&file, path, FA_READ);
    if (res != FR_OK)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Open file failed");
        return;
    }

    /* 先读取固定长度样本，避免一次性加载过大文件 */
    memset(buf, 0, sizeof(buf));
    res = f_read(&file, buf, FILE_VIEW_READ_MAX, &read_len);
    (void)f_close(&file);
    if (res != FR_OK)
    {
        FRONT_COLOR = RED;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Read file failed");
        return;
    }

    /* 初步判断是否“可视文本” */
    for (i = 0; i < read_len; i++)
    {
        uint8_t ch = buf[i];
        if (ch == 0U || (ch < 0x20U && ch != '\r' && ch != '\n' && ch != '\t'))
        {
            likely_text = 0U;
            break;
        }
    }

    /* 粗分类：文本/编码类型 */
    is_utf8 = UI_IsValidUtf8(buf, (uint16_t)read_len);
    /* 分支1：GB2312/ASCII 文本 */
    if (likely_text && !is_utf8)
    {
        buf[read_len] = '\0';
        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)"Content (GB2312/ASCII):");
        FRONT_COLOR = BLACK;
        LCD_ShowTextMixed(8, 104, 304, 188, buf);
    }
    /* 分支2：UTF-8 文本，先转 GB2312 再显示 */
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
    /* 分支3：非文本，按十六进制摘要显示 */
    else
    {
        uint16_t y = 104;
        uint16_t pos = 0;
        FRONT_COLOR = GREEN;
        LCD_ShowString(8, 84, 304, 16, 16, (uint8_t *)(is_utf8 ? "UTF-8 content (hex view):" : "Content (hex, first bytes):"));
        FRONT_COLOR = BLACK;
        while (pos < read_len && y <= 284)
        {
            char hexline[64];
            uint16_t j;
            uint16_t n = (uint16_t)(((read_len - pos) > 8U) ? 8U : (read_len - pos));
            char *cursor = hexline;
            int written = snprintf(cursor, (size_t)(sizeof(hexline) - (size_t)(cursor - hexline)), "%04X:", pos);
            if (written < 0) break;
            cursor += written;
            for (j = 0; j < n; j++)
            {
                written = snprintf(cursor, (size_t)(sizeof(hexline) - (size_t)(cursor - hexline)), " %02X", buf[pos + j]);
                if (written < 0) break;
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

/**
 * @brief 打开当前选中文件
 * @param 无
 * @retval 无
 * @details
 * 功能：将列表页当前选中项切换到查看页显示。
 * 输出：更新 g_view_index、切页并绘制查看页。
 */
void UI_View_OpenSelected(void)
{
    if (g_file_count == 0U || g_selected_index >= g_file_count)
    {
        return;
    }
    g_view_index = g_selected_index;
    UI_EnterPage(UI_PAGE_FILE_VIEW);
    UI_View_Show(g_view_index);
    printf("[KEY2] Open file: %s\r\n", g_file_entries[g_view_index].name);
}
