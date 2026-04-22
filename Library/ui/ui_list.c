#include "ui_list.h"

#include <stdio.h>

#include "../tftlcd/tftlcd.h"
#include "ui_common.h"

/* ============================================================
 * File List Module (ui_list.c)
 * - 负责文件列表页面绘制
 * - 负责选中项上下移动与分页窗口维护
 * ============================================================ */

/* -------------------- static 前置声明区 -------------------- */
static void UI_List_ShowRow(uint16_t row, uint16_t idx, uint8_t selected);

/**
 * @file ui_list.c
 * @brief 文件列表页面模块
 * @details
 * 实现列表页绘制、上下选择、分页窗口移动与防闪烁局部刷新。
 */

/**
 * @brief 绘制列表单行
 * @param row 可视行号（0-based）
 * @param idx 文件缓存索引
 * @param selected 1=选中，0=未选中
 * @retval 无
 * @details
 * 功能：显示一行“箭头+文件名+扩展名+大小”。
 * 输入：列表当前可视窗口行位置与文件索引。
 * 输出：单行区域局部重绘。
 */
static void UI_List_ShowRow(uint16_t row, uint16_t idx, uint8_t selected)
{
    uint16_t y = (uint16_t)(44U + row * 18U);
    char line_mixed[128];
    const char *mark = selected ? ">" : " ";

    /* 先清当前行，再重绘，避免箭头/文本残影 */
    LCD_Fill(0, (uint16_t)(y - 1U), 319, (uint16_t)(y + 16U), WHITE);
    if (idx >= g_file_count) return;

    snprintf(line_mixed, sizeof(line_mixed), "%s%s  .%s  %luB",
             mark, g_file_entries[idx].name, g_file_entries[idx].ext, (unsigned long)g_file_entries[idx].size);
    FRONT_COLOR = selected ? RED : BLACK;
    LCD_ShowTextMixed(4, y, 312, 16, (const uint8_t *)line_mixed);
}

/**
 * @brief 列表页显示/刷新入口
 * @param 无
 * @retval 无
 * @details
 * 功能：根据状态选择整页或局部刷新，显示文件列表与倒计时。
 * 输入：文件缓存、选中项、分页窗口、倒计时状态。
 * 输出：列表页可见区域更新到 LCD。
 */
void UI_ListPage_Show(void)
{
    /* 静态缓存用于判断“本次是否需要局部刷新” */
    static uint8_t inited = 0U;
    static uint16_t last_selected = 0U;
    static uint16_t last_top = 0U;
    static uint16_t last_file_count = 0U;
    static uint8_t last_sd_mounted = 0U;
    static uint32_t last_remain = 0xFFFFFFFFUL;
    uint16_t i;
    uint32_t remain = UI_GetRemainSeconds();
    char line[40];
    uint8_t need_full = 0U;

    /* 触发整页重绘的条件：
     * 1) 首次进入
     * 2) 外部显式请求整页重绘
     * 3) SD 状态或文件总数变化
     */
    if (!inited || g_file_list_need_full_redraw) need_full = 1U;
    if (last_sd_mounted != g_sd_mounted || last_file_count != g_file_count) need_full = 1U;

    if (need_full)
    {
        BACK_COLOR = WHITE;
        LCD_Clear(WHITE);
        FRONT_COLOR = BLUE;
        LCD_ShowString(8, 8, 304, 16, 16, (uint8_t *)"File List (KEY0)");
        LCD_DrawLine(0, 28, 319, 28);
        LCD_Fill(0, 40, 319, 292, WHITE);

        if (!g_sd_mounted)
        {
            FRONT_COLOR = RED;
            LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)"SD not mounted");
        }
        else if (g_file_count == 0U)
        {
            FRONT_COLOR = RED;
            LCD_ShowString(8, 44, 304, 16, 16, (uint8_t *)"No files in root");
        }
        else
        {
            for (i = 0; i < FILE_LIST_PAGE_ROWS; i++)
            {
                uint16_t idx = (uint16_t)(g_list_top_index + i);
                UI_List_ShowRow(i, idx, (idx == g_selected_index) ? 1U : 0U);
            }
        }

        FRONT_COLOR = BLUE;
        LCD_DrawLine(0, 300, 319, 300);
        LCD_ShowString(8, 308, 304, 16, 16, (uint8_t *)"KEY1:Down KEY_UP:Up");
        LCD_ShowString(8, 326, 304, 16, 16, (uint8_t *)"KEY2:Open/Close KEY0:Main");
        last_remain = 0xFFFFFFFFUL;
    }
    else if (g_sd_mounted && g_file_count > 0U)
    {
        /* 非整页路径：尽量只刷新变化区域，减少闪烁 */
        if (last_top != g_list_top_index)
        {
            /* 分页窗口移动：重绘可见行 */
            for (i = 0; i < FILE_LIST_PAGE_ROWS; i++)
            {
                uint16_t idx = (uint16_t)(g_list_top_index + i);
                UI_List_ShowRow(i, idx, (idx == g_selected_index) ? 1U : 0U);
            }
        }
        else if (last_selected != g_selected_index)
        {
            /* 选中项变化：只重绘旧选中行和新选中行 */
            if (last_selected >= g_list_top_index && last_selected < (uint16_t)(g_list_top_index + FILE_LIST_PAGE_ROWS))
            {
                UI_List_ShowRow((uint16_t)(last_selected - g_list_top_index), last_selected, 0U);
            }
            if (g_selected_index >= g_list_top_index && g_selected_index < (uint16_t)(g_list_top_index + FILE_LIST_PAGE_ROWS))
            {
                UI_List_ShowRow((uint16_t)(g_selected_index - g_list_top_index), g_selected_index, 1U);
            }
        }
    }

    /* 倒计时区域单独局部刷新 */
    if (need_full || remain != last_remain)
    {
        LCD_Fill(8, 344, 312, 360, WHITE);
        FRONT_COLOR = BLUE;
        snprintf(line, sizeof(line), "Auto back: %lus", (unsigned long)remain);
        LCD_ShowString(8, 344, 304, 16, 16, (uint8_t *)line);
        last_remain = remain;
    }

    inited = 1U;
    g_file_list_need_full_redraw = 0U;
    last_selected = g_selected_index;
    last_top = g_list_top_index;
    last_file_count = g_file_count;
    last_sd_mounted = g_sd_mounted;
}

/**
 * @brief 列表选中项下移（循环）
 * @param 无
 * @retval 无
 * @details
 * 功能：选中项 +1，超出末尾后回到首项。
 * 输出：更新 g_selected_index 与 g_list_top_index。
 */
void UI_List_SelectDown(void)
{
    if (g_file_count > 0U)
    {
        if ((uint16_t)(g_selected_index + 1U) >= g_file_count) g_selected_index = 0U;
        else g_selected_index++;

        if (g_selected_index < g_list_top_index) g_list_top_index = g_selected_index;
        if (g_selected_index >= g_list_top_index + FILE_LIST_PAGE_ROWS)
        {
            g_list_top_index = g_selected_index - FILE_LIST_PAGE_ROWS + 1U;
        }
    }
}

/**
 * @brief 列表选中项上移（循环）
 * @param 无
 * @retval 无
 * @details
 * 功能：选中项 -1，到首项前回绕到末项。
 * 输出：更新 g_selected_index 与 g_list_top_index。
 */
void UI_List_SelectUp(void)
{
    if (g_file_count > 0U)
    {
        if (g_selected_index == 0U) g_selected_index = (uint16_t)(g_file_count - 1U);
        else g_selected_index--;

        if (g_selected_index < g_list_top_index) g_list_top_index = g_selected_index;
        if (g_selected_index >= g_list_top_index + FILE_LIST_PAGE_ROWS)
        {
            g_list_top_index = g_selected_index - FILE_LIST_PAGE_ROWS + 1U;
        }
    }
}
