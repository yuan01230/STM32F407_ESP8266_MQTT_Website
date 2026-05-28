#include "ui_font_import.h"

#include <stdio.h>
#include <string.h>

#include "../../Core/Inc/main.h"
#include "../tftlcd/tftlcd.h"
#include "../font_storage/font_storage.h"
#include "ui_common.h"

/* ============================================================
 * Font Import Module (ui_font_import.c)
 * - 负责字体导入流程调度（SD -> 外部Flash）
 * - 负责导入进度页面与增量刷新
 * ============================================================ */

/* -------------------- static 前置声明区 -------------------- */
static void UI_Font_ShowProgress(uint8_t percent, uint32_t done, uint32_t total, const char *stage, uint8_t redraw_static);
static void UI_Font_ProgressCb(uint32_t done, uint32_t total, void *user);
static void UI_Font_DoImport(void);

/**
 * @file ui_font_import.c
 * @brief 字库导入页面与导入流程模块
 */

/**
 * @brief 更新导入进度页面
 * @param percent 当前导入百分比
 * @param done 已导入字节数
 * @param total 总字节数
 * @param stage 当前状态文本
 * @param redraw_static 1=重绘静态框架，0=仅刷新动态内容
 * @retval 无
 * @details
 * 功能：显示字库导入状态与进度条。
 * 输入：导入回调实时上报的进度参数。
 * 输出：局部刷新导入页面动态区域。
 * 策略：仅在值变化时刷新对应区域，降低闪烁。
 */
static void UI_Font_ShowProgress(uint8_t percent, uint32_t done, uint32_t total, const char *stage, uint8_t redraw_static)
{
    /* 上次绘制缓存：用于判定是否需要重绘对应动态字段 */
    static uint8_t has_last = 0U;
    static uint8_t last_percent = 0U;
    static uint32_t last_done = 0U;
    static uint32_t last_total = 0U;
    static char last_state[96] = {0};
    uint16_t bar_x = 20;
    uint16_t bar_y = 300;
    uint16_t bar_w = 280;
    uint16_t bar_h = 30;
    uint16_t fill_w = (uint16_t)((bar_w * percent) / 100U);
    uint16_t state_val_x = 72;
    uint16_t state_val_y = 130;
    uint16_t state_val_w = 240;
    uint16_t progress_val_x = 96;
    uint16_t progress_val_y = 220;
    uint16_t progress_val_w = 90;
    uint16_t bytes_val_x = 70;
    uint16_t bytes_val_y = 260;
    uint16_t bytes_val_w = 242;
    char line[96];
    char bytes_line[64];
    const char *state_text = (stage != NULL) ? stage : "State: idle";

    if (redraw_static)
    {
        /* 首帧：绘制静态框架与标签（后续不重复绘制） */
        has_last = 0U;
        last_state[0] = '\0';
        BACK_COLOR = WHITE;
        LCD_Clear(WHITE);
        FRONT_COLOR = BLUE;
        LCD_ShowString(8, 10, 304, 16, 16, (uint8_t *)"FONT IMPORT");
        LCD_DrawLine(0, 30, 319, 30);

        FRONT_COLOR = BLACK;
        LCD_ShowString(8, 46, 304, 16, 16, (uint8_t *)"[Part1] Info");
        LCD_ShowString(8, 74, 304, 16, 16, (uint8_t *)"Path: 0:/Font/HZK16");
        LCD_ShowString(8, 102, 304, 16, 16, (uint8_t *)"Tip : Keep power on");
        LCD_ShowString(8, 130, 60, 16, 16, (uint8_t *)"State:");

        FRONT_COLOR = BLUE;
        LCD_DrawLine(0, 170, 319, 170);
        FRONT_COLOR = BLACK;
        LCD_ShowString(8, 182, 304, 16, 16, (uint8_t *)"[Part2] Progress");
        LCD_ShowString(8, 220, 86, 16, 16, (uint8_t *)"Progress:");
        LCD_ShowString(8, 260, 60, 16, 16, (uint8_t *)"Bytes:");
        LCD_DrawRectangle(bar_x, bar_y, (uint16_t)(bar_x + bar_w), (uint16_t)(bar_y + bar_h));
    }

    /* 状态文本仅在变化时刷新 */
    if (!has_last || strcmp(last_state, state_text) != 0)
    {
        LCD_Fill(state_val_x, (uint16_t)(state_val_y - 2U), (uint16_t)(state_val_x + state_val_w), (uint16_t)(state_val_y + 18U), WHITE);
        snprintf(line, sizeof(line), "%s", state_text);
        FRONT_COLOR = BLACK;
        LCD_ShowString(state_val_x, state_val_y, state_val_w, 16, 16, (uint8_t *)line);
        strncpy(last_state, state_text, sizeof(last_state) - 1U);
        last_state[sizeof(last_state) - 1U] = '\0';
    }

    /* 字节统计仅在变化时刷新 */
    if (!has_last || last_done != done || last_total != total)
    {
        LCD_Fill(bytes_val_x, (uint16_t)(bytes_val_y - 2U), (uint16_t)(bytes_val_x + bytes_val_w), (uint16_t)(bytes_val_y + 18U), WHITE);
        snprintf(bytes_line, sizeof(bytes_line), "%lu / %lu", (unsigned long)done, (unsigned long)total);
        FRONT_COLOR = BLACK;
        LCD_ShowString(bytes_val_x, bytes_val_y, bytes_val_w, 16, 16, (uint8_t *)bytes_line);
    }

    /* 百分比变化时刷新百分比文本和进度条填充 */
    if (!has_last || last_percent != percent)
    {
        LCD_Fill(progress_val_x, (uint16_t)(progress_val_y - 2U), (uint16_t)(progress_val_x + progress_val_w), (uint16_t)(progress_val_y + 18U), WHITE);
        snprintf(line, sizeof(line), "%u%%", (unsigned int)percent);
        FRONT_COLOR = BLACK;
        LCD_ShowString(progress_val_x, progress_val_y, progress_val_w, 16, 16, (uint8_t *)line);
        LCD_Fill((uint16_t)(bar_x + 1U), (uint16_t)(bar_y + 1U), (uint16_t)(bar_x + bar_w - 1U), (uint16_t)(bar_y + bar_h - 1U), WHITE);
        if (fill_w > 0U)
        {
            LCD_Fill((uint16_t)(bar_x + 1U), (uint16_t)(bar_y + 1U), (uint16_t)(bar_x + fill_w - 1U), (uint16_t)(bar_y + bar_h - 1U), GREEN);
        }
    }

    has_last = 1U;
    last_percent = percent;
    last_done = done;
    last_total = total;
}

/**
 * @brief 字库导入回调
 * @param done 当前已导入字节
 * @param total 总字节
 * @param user 用户上下文（用于保存上次百分比）
 * @retval 无
 * @details
 * 功能：将底层导入进度转换为 UI 可显示格式并刷新页面。
 */
static void UI_Font_ProgressCb(uint32_t done, uint32_t total, void *user)
{
    uint8_t percent;
    uint8_t *last_percent = (uint8_t *)user;

    /* 计算百分比并限幅到 0~100 */
    if (total == 0U) percent = 0U;
    else
    {
        percent = (uint8_t)((done * 100UL) / total);
        if (percent > 100U) percent = 100U;
    }

    if (last_percent != NULL) *last_percent = percent;
    UI_Font_ShowProgress(percent, done, total, "State: importing SD -> Flash", 0U);
}

/**
 * @brief 执行一次字库导入
 * @param 无
 * @retval 无
 * @details
 * 功能：执行 SD -> 外部 Flash 的字库导入流程。
 * 说明：该函数被“启动初始化导入”和“主页面手动重导”共用。
 */
static void UI_Font_DoImport(void)
{
    FontStorageResult import_res;
    uint8_t last_percent = 0xFFU;

    /* 未挂载 SD 时直接返回，避免无效导入 */
    if (!g_sd_mounted)
    {
        printf("[KEY_UP] SD not mounted, skip font import\r\n");
        return;
    }

    printf("[Font] Importing font from SD (%s) to external flash...\r\n", FONT_SD_PATH);
    UI_Font_ShowProgress(0U, 0U, 0U, "State: prepare import", 1U);
    /* 执行导入，回调负责实时更新进度页面 */
    import_res = FontStorage_ImportFromSDEx(FONT_SD_PATH, UI_Font_ProgressCb, &last_percent);
    if (import_res == FONT_STORAGE_OK)
    {
        UI_Font_ShowProgress(100U, FontStorage_GetFontSize(), FontStorage_GetFontSize(), "State: import done", 0U);
        HAL_Delay(800);
        printf("[Font] Import success, size=%lu bytes\r\n", FontStorage_GetFontSize());
    }
    else
    {
        UI_Font_ShowProgress(last_percent == 0xFFU ? 0U : last_percent, 0U, 0U, "State: import failed", 0U);
        HAL_Delay(800);
        printf("[Font] Import failed, code=%d\r\n", (int)import_res);
    }
}

/**
 * @brief 启动阶段字库初始化
 * @param sd_ready SD 是否可用（1=可用）
 * @retval 无
 * @details
 * 功能：若外部 Flash 字库不可用且 SD 可用，则执行导入。
 */
void UI_Font_ImportSystemInit(uint8_t sd_ready)
{
    FontStorage_Init();
    if (FontStorage_IsReady())
    {
        printf("[Font] External flash font is ready, size=%lu bytes\r\n", FontStorage_GetFontSize());
        return;
    }
    if (!sd_ready)
    {
        printf("[Font] No SD card mounted, skip font import\r\n");
        return;
    }
    UI_Font_DoImport();
}

/**
 * @brief 主页面触发手动重导入
 * @param 无
 * @retval 无
 */
void UI_Font_RunImportFromMain(void)
{
    FontStorage_Init();
    UI_Font_DoImport();
}
