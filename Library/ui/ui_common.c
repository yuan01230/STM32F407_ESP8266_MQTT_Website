#include "ui_common.h"

#include <string.h>

#include "../../Core/Inc/main.h"
#include "../../FATFS/App/fatfs.h"
#include "../tftlcd/tftlcd.h"

/* ============================================================
 * UI Common Layer (ui_common.c)
 * - 维护跨页面共享状态
 * - 提供文件列表缓存与编码转换基础能力
 * ============================================================ */

/* -------------------- static 前置声明区 -------------------- */
/* 本文件无内部 static 函数 */

/**
 * @file ui_common.c
 * @brief UI 底层公共层
 * @details
 * 存放跨页面共享状态与可复用基础函数：
 * - 页面状态与计时
 * - SD 根目录文件列表缓存
 * - UTF-8 识别与 UTF-8 -> GB2312 转换
 */

/* -------------------- 全局共享状态 -------------------- */
/* 由 UI_SetSdMounted 写入；各页面读取显示/流程判断 */
uint8_t g_sd_mounted = 0;
/* 由 KEY1(主页面) 切换；主页面绘制函数读取决定字体颜色 */
uint8_t g_test_fg_mode = 0;
/* 当前页面状态，主要由 UI_EnterPage/UI_HandleKey 改变 */
UiPage_t g_ui_page = UI_PAGE_MAIN;
/* 最近一次用户操作时间戳，UI_Tick 用于计算 30s 超时返回 */
uint32_t g_ui_last_action_tick = 0;
/* 主页面显示用的 SD 根目录文件总数 */
uint16_t g_main_file_count = 0;
/* UI 秒级刷新节流变量，避免同一秒重复刷新 */
uint32_t g_last_sec = 0xFFFFFFFFUL;
/* 列表页文件缓存 */
FileEntry_t g_file_entries[MAX_FILE_ENTRIES];
/* 当前缓存中的文件数量 */
uint16_t g_file_count = 0;
/* 列表页当前选中项索引 */
uint16_t g_selected_index = 0;
/* 列表页当前窗口顶部索引（分页显示起点） */
uint16_t g_list_top_index = 0;
/* 查看页当前打开文件索引 */
uint16_t g_view_index = 0;
/* 列表页强制整页重绘标记，置 1 后下次 UI_ListPage_Show 做全量绘制 */
uint8_t g_file_list_need_full_redraw = 1U;

/**
 * @brief 页面切换公共入口
 * @param page 目标页面
 * @retval 无
 * @details
 * 功能：统一处理页面切换时的公共状态更新。
 * 输入：目标页面枚举值。
 * 输出：更新当前页面、最后操作时间及必要重绘标志。
 */
void UI_EnterPage(UiPage_t page)
{
    /* 切页时统一刷新“最后操作时间”，用于超时返回计算 */
    g_ui_page = page;
    g_ui_last_action_tick = HAL_GetTick();
    if (page == UI_PAGE_FILE_LIST)
    {
        /* 进入列表页时先整页绘制一次，避免残影 */
        g_file_list_need_full_redraw = 1U;
    }
}

/**
 * @brief 获取系统秒计数
 * @param 无
 * @retval 当前运行秒数
 * @details
 * 功能：为 UI 秒级刷新、运行时间显示提供时间基准。
 */
uint32_t UI_GetCurrentSeconds(void)
{
    return HAL_GetTick() / 1000U;
}

/**
 * @brief 计算自动返回剩余秒数
 * @param 无
 * @retval 距离自动返回主页面的剩余秒数
 * @details
 * 功能：用于列表页/查看页底部倒计时显示。
 */
uint32_t UI_GetRemainSeconds(void)
{
    uint32_t elapsed = HAL_GetTick() - g_ui_last_action_tick;
    if (elapsed >= UI_AUTO_RETURN_MS)
    {
        return 0U;
    }
    return (UI_AUTO_RETURN_MS - elapsed + 999U) / 1000U;
}

/**
 * @brief 统计根目录文件数
 * @param 无
 * @retval 普通文件数量（不含目录）
 * @details
 * 功能：供主页面显示“SD Files”统计信息。
 */
uint16_t UI_CountRootFiles(void)
{
    DIR dir;
    FILINFO fno;
    uint16_t count = 0;
    if (!g_sd_mounted) return 0;
    if (f_opendir(&dir, "0:/") != FR_OK) return 0;
    while (count < 0xFFFFU)
    {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0') break;
        if ((fno.fattrib & AM_DIR) != 0U) continue;
        count++;
    }
    (void)f_closedir(&dir);
    return count;
}

/**
 * @brief 加载根目录文件列表
 * @param 无
 * @retval 实际加载的文件数量
 * @details
 * 功能：刷新列表页数据源缓存。
 * 输入：SD 根目录文件系统状态。
 * 输出：更新 g_file_entries/g_file_count/g_selected_index/g_list_top_index。
 */
uint16_t UI_LoadRootFileList(void)
{
    DIR dir;
    FILINFO fno;
    uint16_t idx = 0;
    if (!g_sd_mounted)
    {
        g_file_count = 0;
        g_selected_index = 0;
        g_list_top_index = 0;
        return 0;
    }

    if (f_opendir(&dir, "0:/") != FR_OK)
    {
        g_file_count = 0;
        return 0;
    }

    while (idx < MAX_FILE_ENTRIES)
    {
        const char *src_name;
        char *dot;
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0') break;
        if ((fno.fattrib & AM_DIR) != 0U) continue;

        memset(&g_file_entries[idx], 0, sizeof(g_file_entries[idx]));
        src_name = (fno.fname[0] != '\0') ? fno.fname : fno.altname;
        strncpy(g_file_entries[idx].name, src_name, sizeof(g_file_entries[idx].name) - 1U);
        g_file_entries[idx].size = fno.fsize;
        g_file_entries[idx].is_dir = 0U;

        /* 从文件名中提取扩展名（不含点） */
        dot = strrchr(g_file_entries[idx].name, '.');
        if (dot != NULL && *(dot + 1) != '\0')
        {
            strncpy(g_file_entries[idx].ext, dot + 1, sizeof(g_file_entries[idx].ext) - 1U);
        }
        else
        {
            strncpy(g_file_entries[idx].ext, "N/A", sizeof(g_file_entries[idx].ext) - 1U);
        }
        idx++;
    }
    (void)f_closedir(&dir);

    g_file_count = idx;
    /* 保证选中索引始终有效，避免列表为空/越界后显示异常 */
    if (g_file_count == 0U)
    {
        g_selected_index = 0U;
        g_list_top_index = 0U;
    }
    else if (g_selected_index >= g_file_count)
    {
        g_selected_index = 0U;
        g_list_top_index = 0U;
    }
    return g_file_count;
}

/**
 * @brief 判断缓冲区是否为 UTF-8
 * @param buf 输入字节缓冲区
 * @param len 缓冲区长度
 * @retval 1=有效 UTF-8，0=无效
 * @details
 * 功能：为文本显示分支选择提供依据。
 */
uint8_t UI_IsValidUtf8(const uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;
    uint8_t seen_multibyte = 0;
    while (i < len)
    {
        uint8_t c = buf[i];
        uint8_t need = 0;
        if (c < 0x80U) { i++; continue; }
        if ((c & 0xE0U) == 0xC0U) { if (c < 0xC2U) return 0; need = 1; }
        else if ((c & 0xF0U) == 0xE0U) need = 2;
        else if ((c & 0xF8U) == 0xF0U) { if (c > 0xF4U) return 0; need = 3; }
        else return 0;
        if ((uint16_t)(i + need) >= len) return 0;
        for (uint8_t j = 1; j <= need; j++) if ((buf[i + j] & 0xC0U) != 0x80U) return 0;
        seen_multibyte = 1;
        i = (uint16_t)(i + need + 1U);
    }
    return seen_multibyte;
}

/**
 * @brief 解码单个 UTF-8 码点
 * @param in 输入起始地址
 * @param in_len 可用输入长度
 * @param consumed 输出消耗字节数
 * @param codepoint 输出 Unicode 码点
 * @retval 1=解码成功，0=失败
 */
uint8_t UI_Utf8DecodeCodepoint(const uint8_t *in, uint16_t in_len, uint16_t *consumed, uint32_t *codepoint)
{
    uint8_t c0;
    if (in == NULL || consumed == NULL || codepoint == NULL || in_len == 0U) return 0;
    c0 = in[0];
    if (c0 < 0x80U) { *codepoint = c0; *consumed = 1U; return 1; }
    if ((c0 & 0xE0U) == 0xC0U && in_len >= 2U)
    {
        uint8_t c1 = in[1];
        if ((c1 & 0xC0U) != 0x80U || c0 < 0xC2U) return 0;
        *codepoint = ((uint32_t)(c0 & 0x1FU) << 6) | (uint32_t)(c1 & 0x3FU);
        *consumed = 2U;
        return 1;
    }
    if ((c0 & 0xF0U) == 0xE0U && in_len >= 3U)
    {
        uint8_t c1 = in[1];
        uint8_t c2 = in[2];
        uint32_t cp;
        if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U) return 0;
        cp = ((uint32_t)(c0 & 0x0FU) << 12) | ((uint32_t)(c1 & 0x3FU) << 6) | (uint32_t)(c2 & 0x3FU);
        if (cp < 0x800U || (cp >= 0xD800U && cp <= 0xDFFFU)) return 0;
        *codepoint = cp;
        *consumed = 3U;
        return 1;
    }
    if ((c0 & 0xF8U) == 0xF0U && in_len >= 4U)
    {
        uint8_t c1 = in[1];
        uint8_t c2 = in[2];
        uint8_t c3 = in[3];
        uint32_t cp;
        if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U || (c3 & 0xC0U) != 0x80U) return 0;
        cp = ((uint32_t)(c0 & 0x07U) << 18) | ((uint32_t)(c1 & 0x3FU) << 12) | ((uint32_t)(c2 & 0x3FU) << 6) | (uint32_t)(c3 & 0x3FU);
        if (cp < 0x10000U || cp > 0x10FFFFU) return 0;
        *codepoint = cp;
        *consumed = 4U;
        return 1;
    }
    return 0;
}

/**
 * @brief UTF-8 字节流转 GB2312
 * @param in UTF-8 输入缓冲区
 * @param in_len 输入长度
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=有有效输出，0=转换失败
 * @details
 * 功能：将 UTF-8 文本转换为 LCD/FatFs 兼容的 GB2312 字节流。
 * 说明：无法映射的字符统一输出 '?'。
 */
uint8_t UI_Utf8ToGb2312(const uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t out_cap)
{
    uint16_t i = 0;
    uint16_t o = 0;
    while (i < in_len && o + 1U < out_cap)
    {
        uint8_t c = in[i];
        if (c < 0x80U)
        {
            out[o++] = c;
            i++;
            continue;
        }

        uint16_t consumed = 0;
        uint32_t cp = 0;
        if (UI_Utf8DecodeCodepoint(&in[i], (uint16_t)(in_len - i), &consumed, &cp))
        {
            if (cp <= 0xFFFFU)
            {
                WCHAR oem = ff_convert((WCHAR)cp, 0U);
                if (oem != 0U)
                {
                    if (oem < 0x100U) out[o++] = (uint8_t)oem;
                    else
                    {
                        if (o + 2U >= out_cap) break;
                        out[o++] = (uint8_t)((oem >> 8) & 0xFFU);
                        out[o++] = (uint8_t)(oem & 0xFFU);
                    }
                }
                else out[o++] = '?';
            }
            else out[o++] = '?';
            i = (uint16_t)(i + consumed);
            continue;
        }

        out[o++] = '?';
        i++;
    }
    out[o] = '\0';
    return (o > 0U) ? 1U : 0U;
}

/**
 * @brief UTF-8 C 字符串转 GB2312
 * @param utf8 输入 UTF-8 字符串
 * @param out 输出 GB2312 缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=成功，0=失败
 */
uint8_t UI_TextUtf8ToGb2312(const char *utf8, uint8_t *out, uint16_t out_cap)
{
    if (utf8 == NULL || out == NULL || out_cap == 0U)
    {
        return 0U;
    }
    return UI_Utf8ToGb2312((const uint8_t *)utf8, (uint16_t)strlen(utf8), out, out_cap);
}

/**
 * @brief 直接显示 UTF-8 字符串到 LCD
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param width 显示区域宽度
 * @param height 显示区域高度
 * @param utf8 UTF-8 字符串
 * @retval 无
 * @details
 * 流程：UTF-8 -> GB2312 -> LCD_ShowTextMixed。
 * 若转换失败，回退到 LCD_ShowString 显示原串。
 */
void UI_LCD_ShowUtf8(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *utf8)
{
    uint8_t gb[256];
    if (utf8 == NULL)
    {
        return;
    }
    if (UI_TextUtf8ToGb2312(utf8, gb, (uint16_t)sizeof(gb)))
    {
        LCD_ShowTextMixed(x, y, width, height, gb);
    }
    else
    {
        LCD_ShowString(x, y, width, height, 16, (uint8_t *)utf8);
    }
}
