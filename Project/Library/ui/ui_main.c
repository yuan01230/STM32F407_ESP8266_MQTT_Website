#include "ui_main.h"

#include <stdio.h>

#include "../tftlcd/tftlcd.h"
#include "../font_storage/font_storage.h"
#include "ui_common.h"

/* ============================================================
 * Main Page Module (ui_main.c)
 * - 负责主页面整体布局绘制
 * - 负责主页面运行时间等局部刷新
 * ============================================================ */

/* -------------------- static 前置声明区 -------------------- */
/* 本文件无内部 static 函数 */

/**
 * @file ui_main.c
 * @brief 主页面显示模块
 */

/**
 * @brief 绘制主页面
 * @param 无
 * @retval 无
 * @details
 * 功能：完整绘制主页面静态与动态文本区域。
 * 输入：颜色模式、SD状态、字库状态、文件总数等全局状态。
 * 输出：主页面布局与文本刷新到 LCD。
 * 流程：
 * 1. 清屏并确定当前文本颜色；
 * 2. 绘制标题与分割线；
 * 3. 绘制中英文混排示例；
 * 4. 绘制按键提示与状态区。
 */
void UI_MainPage_Show(void)
{
    static const char zh_row[] = "[中] Chinese: 中文显示测试";
    static const char en_row[] = "[A] English: English rendering test";
    static const char mixed_row1[] = "[混] Mixed 1: ABC123 + 中文";
    static const char mixed_row2[] = "[混] Mixed 2: 英文 + English + 2026";
    char line[64];
    uint16_t text_color;

    /* 每次进入主页面都做整页重绘，保证布局一致 */
    BACK_COLOR = WHITE;
    LCD_Clear(WHITE);

    /* 颜色模式统一作用于主页面文本 */
    if (g_test_fg_mode == 0U) text_color = BLACK;
    else if (g_test_fg_mode == 1U) text_color = BLUE;
    else text_color = RED;

    /* 标题与正文使用当前颜色模式 */
    FRONT_COLOR = text_color;
    LCD_ShowString(6, 4, 308, 16, 16, (uint8_t *)"LCD Font Test Case");
    /* 分割线固定颜色，避免颜色切换时界面结构弱化 */
    FRONT_COLOR = BLACK;
    LCD_DrawLine(0, 22, 319, 22);

    /* 状态区文本同样跟随颜色模式 */
    FRONT_COLOR = text_color;
    UI_LCD_ShowUtf8(4, 30, 312, 16, zh_row);
    UI_LCD_ShowUtf8(4, 60, 312, 16, en_row);
    UI_LCD_ShowUtf8(4, 90, 312, 16, mixed_row1);
    UI_LCD_ShowUtf8(4, 120, 312, 16, mixed_row2);

    FRONT_COLOR = BLACK;
    LCD_DrawLine(0, 156, 319, 156);
    FRONT_COLOR = text_color;
    UI_LCD_ShowUtf8(4, 168, 154, 16, "[K0] List/Back");
    UI_LCD_ShowUtf8(164, 168, 152, 16, "[K1] Color");
    UI_LCD_ShowUtf8(4, 192, 154, 16, "[K2] Create");
    UI_LCD_ShowUtf8(164, 192, 152, 16, "[KU] Reload");

    FRONT_COLOR = BLACK;
    LCD_DrawLine(0, 228, 319, 228);
    FRONT_COLOR = text_color;
    snprintf(line, sizeof(line), "SD: %s", g_sd_mounted ? "mounted" : "not mounted");
    LCD_ShowString(4, 236, 312, 16, 16, (uint8_t *)line);
    snprintf(line, sizeof(line), "Font: %s", FontStorage_IsReady() ? "ready" : "not ready");
    LCD_ShowString(4, 260, 312, 16, 16, (uint8_t *)line);
    snprintf(line, sizeof(line), "SD Files: %u", (unsigned int)g_main_file_count);
    LCD_ShowString(4, 284, 312, 16, 16, (uint8_t *)line);
    LCD_DrawLine(0, 446, 319, 446);
}

/**
 * @brief 刷新主页面运行时间
 * @param seconds 当前运行秒数
 * @retval 无
 * @details
 * 功能：更新主页面底部运行时间。
 * 输入：秒计数。
 * 输出：仅底部时钟区域重绘（局部刷新）。
 */
void UI_MainPage_UpdateRuntime(uint32_t seconds)
{
    char line[48];
    /* 仅清理底部小区域，避免整屏闪烁 */
    FRONT_COLOR = BLUE;
    LCD_Fill(4, 454, 319, 474, WHITE);
    snprintf(line, sizeof(line), "Runtime: %lus", (unsigned long)seconds);
    LCD_ShowString(4, 456, 312, 16, 16, (uint8_t *)line);
}
