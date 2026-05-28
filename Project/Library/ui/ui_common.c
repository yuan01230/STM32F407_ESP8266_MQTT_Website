#include "ui_common.h"

#include <stdio.h>
#include <string.h>

#include "../../Core/Inc/main.h"
#include "../../FATFS/App/fatfs.h"
#include "../tftlcd/tftlcd.h"

/* ============================================================
 * UI Common Layer (ui_common.c)
 * - 维护跨页面共享状态
 * - 提供通用文件浏览器状态与路径处理能力
 * - 提供 UTF-8/GB2312 转换基础能力
 * ============================================================ */

/* -------------------- 当前文件内部辅助函数声明 -------------------- */
static uint8_t UI_IsRootPath(const char *path);
static uint8_t UI_CopyNormalizedPath(const char *src, char *dst, uint16_t dst_cap);
static uint8_t UI_BuildPath(const char *base, const char *name, char *out, uint16_t out_cap);
static uint8_t UI_BuildParentPath(const char *current, char *out, uint16_t out_cap);
static uint16_t UI_CountEntriesInDirectory(const char *dir_path);

/**
 * @file ui_common.c
 * @brief UI 底层公共层
 * @details
 * 本文件负责：
 * 1. 维护跨页面共享状态；
 * 2. 维护通用文件浏览器的当前目录；
 * 3. 生成虚拟父目录项 `[..]`；
 * 4. 为列表页、按键分发层、查看页提供统一的路径和目录操作；
 * 5. 提供 UTF-8 识别与 UTF-8 -> GB2312 转换能力。
 */

/* -------------------- 全局共享状态 -------------------- */
uint8_t g_sd_mounted = 0;
uint8_t g_test_fg_mode = 0;
UiPage_t g_ui_page = UI_PAGE_MAIN;
uint32_t g_ui_last_action_tick = 0;
uint16_t g_main_file_count = 0;
uint32_t g_last_sec = 0xFFFFFFFFUL;
FileEntry_t g_file_entries[MAX_FILE_ENTRIES];
char g_file_browse_path[UI_FILE_PATH_MAX] = UI_ROOT_DIR_PATH;
uint16_t g_file_count = 0;
uint16_t g_selected_index = 0;
uint16_t g_list_top_index = 0;
uint16_t g_view_index = 0;
uint8_t g_file_list_need_full_redraw = 1U;

/**
 * @brief 判断某个路径是否为根目录
 * @param path 待判断路径
 * @retval 1=根目录，0=非根目录
 * @details
 * 浏览器内部统一把根目录视为 `0:/`。
 * 为了兼容外部可能传入的 `0:`，这里额外接受这两种等价写法。
 */
static uint8_t UI_IsRootPath(const char *path)
{
    if (path == NULL)
    {
        return 0U;
    }

    return (uint8_t)((strcmp(path, UI_ROOT_DIR_PATH) == 0) || (strcmp(path, "0:") == 0));
}

/**
 * @brief 复制并规范化目录路径
 * @param src 输入路径
 * @param dst 输出缓冲区
 * @param dst_cap 输出缓冲区容量
 * @retval 1=成功，0=失败
 * @details
 * 规范化规则：
 * 1. `0:` 会被转换为 `0:/`；
 * 2. 非根目录路径末尾多余的 `/` 会被移除；
 * 3. 根目录始终保持为 `0:/`。
 */
static uint8_t UI_CopyNormalizedPath(const char *src, char *dst, uint16_t dst_cap)
{
    size_t len;

    if (src == NULL || dst == NULL || dst_cap == 0U)
    {
        return 0U;
    }

    if (strcmp(src, "0:") == 0)
    {
        return (uint8_t)(snprintf(dst, dst_cap, "%s", UI_ROOT_DIR_PATH) < (int)dst_cap);
    }

    if (snprintf(dst, dst_cap, "%s", src) >= (int)dst_cap)
    {
        return 0U;
    }

    len = strlen(dst);
    while (len > strlen(UI_ROOT_DIR_PATH) && dst[len - 1U] == '/')
    {
        dst[len - 1U] = '\0';
        len--;
    }

    if (UI_IsRootPath(dst))
    {
        (void)snprintf(dst, dst_cap, "%s", UI_ROOT_DIR_PATH);
    }

    return 1U;
}

/**
 * @brief 由“目录路径 + 名称”拼出完整路径
 * @param base 基础目录
 * @param name 目录项名称
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=成功，0=失败
 * @details
 * 该函数同时服务于：
 * 1. 进入子目录时拼目录路径；
 * 2. 打开普通文件时拼文件路径。
 */
static uint8_t UI_BuildPath(const char *base, const char *name, char *out, uint16_t out_cap)
{
    size_t base_len;

    if (base == NULL || name == NULL || out == NULL || out_cap == 0U)
    {
        return 0U;
    }

    base_len = strlen(base);
    if (base_len > 0U && base[base_len - 1U] == '/')
    {
        return (uint8_t)(snprintf(out, out_cap, "%s%s", base, name) < (int)out_cap);
    }

    return (uint8_t)(snprintf(out, out_cap, "%s/%s", base, name) < (int)out_cap);
}

/**
 * @brief 计算当前目录的父目录路径
 * @param current 当前目录
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=成功，0=失败
 * @details
 * 规则如下：
 * 1. 如果当前已经是根目录，则父目录仍然是根目录；
 * 2. 如果当前在子目录，则删除最后一个路径段；
 * 3. 删除后若退回盘符根，则统一写成 `0:/`。
 */
static uint8_t UI_BuildParentPath(const char *current, char *out, uint16_t out_cap)
{
    char temp[UI_FILE_PATH_MAX];
    char *last_slash;

    if (!UI_CopyNormalizedPath(current, temp, sizeof(temp)))
    {
        return 0U;
    }

    if (UI_IsRootPath(temp))
    {
        return (uint8_t)(snprintf(out, out_cap, "%s", UI_ROOT_DIR_PATH) < (int)out_cap);
    }

    last_slash = strrchr(temp, '/');
    if (last_slash == NULL)
    {
        return (uint8_t)(snprintf(out, out_cap, "%s", UI_ROOT_DIR_PATH) < (int)out_cap);
    }

    if (last_slash <= &temp[2])
    {
        return (uint8_t)(snprintf(out, out_cap, "%s", UI_ROOT_DIR_PATH) < (int)out_cap);
    }

    *last_slash = '\0';
    return UI_CopyNormalizedPath(temp, out, out_cap);
}

/**
 * @brief 统计某个目录中的可见条目数量
 * @param dir_path 目录路径
 * @retval 条目数量
 * @details
 * 这里统计的是 FATFS 中真实存在的目录和文件。
 * 不把 `.` 和 `..` 计入数量，也不在这里生成虚拟父目录项。
 */
static uint16_t UI_CountEntriesInDirectory(const char *dir_path)
{
    DIR dir;
    FILINFO fno;
    uint16_t count = 0;

    if (dir_path == NULL || dir_path[0] == '\0')
    {
        return 0U;
    }

    if (f_opendir(&dir, dir_path) != FR_OK)
    {
        return 0U;
    }

    while (count < 0xFFFFU)
    {
        const char *src_name;

        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0')
        {
            break;
        }

        src_name = (fno.fname[0] != '\0') ? fno.fname : fno.altname;
        if (strcmp(src_name, ".") == 0 || strcmp(src_name, "..") == 0)
        {
            continue;
        }

        count++;
    }

    (void)f_closedir(&dir);
    return count;
}

/**
 * @brief 进入指定页面并刷新无操作计时
 * @param page 目标页面
 */
void UI_EnterPage(UiPage_t page)
{
    g_ui_page = page;
    g_ui_last_action_tick = HAL_GetTick();
    if (page == UI_PAGE_FILE_LIST)
    {
        g_file_list_need_full_redraw = 1U;
    }
}

/**
 * @brief 获取系统秒计数
 * @retval 当前运行秒数
 */
uint32_t UI_GetCurrentSeconds(void)
{
    return HAL_GetTick() / 1000U;
}

/**
 * @brief 计算自动返回剩余秒数
 * @retval 距离自动返回主页面的剩余秒数
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
 * @brief 统计根目录中的可见条目数量
 * @retval 条目数量
 * @details
 * 该接口供主页面显示使用，不应改变当前浏览器所在目录。
 */
uint16_t UI_CountRootFiles(void)
{
    if (!g_sd_mounted)
    {
        return 0U;
    }

    return UI_CountEntriesInDirectory(UI_ROOT_DIR_PATH);
}

/**
 * @brief 加载指定目录到文件列表缓存
 * @param path 目标目录路径
 * @retval 1=成功，0=失败
 * @details
 * 该函数会完整重建当前列表：
 * 1. 打开目标目录；
 * 2. 根据是否为根目录决定是否插入 `[..]`；
 * 3. 读取目录和文件项；
 * 4. 刷新当前目录状态和列表索引。
 */
uint8_t UI_LoadDirectory(const char *path)
{
    DIR dir;
    FILINFO fno;
    char normalized_path[UI_FILE_PATH_MAX];
    uint16_t idx = 0U;

    if (!g_sd_mounted)
    {
        g_file_count = 0U;
        g_selected_index = 0U;
        g_list_top_index = 0U;
        return 0U;
    }

    if (!UI_CopyNormalizedPath(path, normalized_path, sizeof(normalized_path)))
    {
        return 0U;
    }

    if (f_opendir(&dir, normalized_path) != FR_OK)
    {
        return 0U;
    }

    if (!UI_IsRootPath(normalized_path) && idx < MAX_FILE_ENTRIES)
    {
        memset(&g_file_entries[idx], 0, sizeof(g_file_entries[idx]));
        strncpy(g_file_entries[idx].name, "..", sizeof(g_file_entries[idx].name) - 1U);
        g_file_entries[idx].is_dir = 1U;
        g_file_entries[idx].type = UI_ENTRY_PARENT;
        idx++;
    }

    while (idx < MAX_FILE_ENTRIES)
    {
        const char *src_name;
        char *dot;

        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0')
        {
            break;
        }

        src_name = (fno.fname[0] != '\0') ? fno.fname : fno.altname;
        if (strcmp(src_name, ".") == 0 || strcmp(src_name, "..") == 0)
        {
            continue;
        }

        memset(&g_file_entries[idx], 0, sizeof(g_file_entries[idx]));
        strncpy(g_file_entries[idx].name, src_name, sizeof(g_file_entries[idx].name) - 1U);

        if ((fno.fattrib & AM_DIR) != 0U)
        {
            /* 目录项只记录名称和类型，大小与扩展名在列表页中都不会使用。 */
            g_file_entries[idx].is_dir = 1U;
            g_file_entries[idx].type = UI_ENTRY_DIR;
        }
        else
        {
            /* 文件项保留大小和扩展名，供列表显示和文件类型判断使用。 */
            g_file_entries[idx].size = fno.fsize;
            g_file_entries[idx].is_dir = 0U;
            g_file_entries[idx].type = UI_ENTRY_FILE;
            dot = strrchr(g_file_entries[idx].name, '.');
            if (dot != NULL && *(dot + 1) != '\0')
            {
                strncpy(g_file_entries[idx].ext, dot + 1, sizeof(g_file_entries[idx].ext) - 1U);
            }
            else
            {
                strncpy(g_file_entries[idx].ext, "N/A", sizeof(g_file_entries[idx].ext) - 1U);
            }
        }
        idx++;
    }

    (void)f_closedir(&dir);

    (void)snprintf(g_file_browse_path, sizeof(g_file_browse_path), "%s", normalized_path);
    g_file_count = idx;
    g_selected_index = 0U;
    g_list_top_index = 0U;
    return 1U;
}

/**
 * @brief 加载根目录列表到缓存
 * @retval 加载后的条目数量
 */
uint16_t UI_LoadRootFileList(void)
{
    return UI_LoadDirectory(UI_ROOT_DIR_PATH) ? g_file_count : 0U;
}

/**
 * @brief 获取当前浏览器目录
 * @retval 当前目录路径
 */
const char *UI_GetCurrentDir(void)
{
    return g_file_browse_path;
}

/**
 * @brief 判断当前目录是否为根目录
 * @retval 1=当前就是根目录，0=不是根目录
 */
uint8_t UI_IsRootDir(void)
{
    return UI_IsRootPath(g_file_browse_path);
}

/**
 * @brief 打开当前选中条目
 * @retval UiOpenResult_t 打开结果
 * @details
 * 该函数会根据当前选中项类型决定下一步动作：
 * 1. `[..]` -> 计算父目录并装载父目录；
 * 2. 目录项 -> 构造子目录路径并装载子目录；
 * 3. 文件项 -> 不改目录状态，只通知调用方进入查看页。
 */
UiOpenResult_t UI_OpenSelectedEntry(void)
{
    char next_path[UI_FILE_PATH_MAX];

    if (g_file_count == 0U || g_selected_index >= g_file_count)
    {
        return UI_OPEN_RESULT_NONE;
    }

    switch (g_file_entries[g_selected_index].type)
    {
        case UI_ENTRY_PARENT:
            if (UI_BuildParentPath(g_file_browse_path, next_path, sizeof(next_path)) && UI_LoadDirectory(next_path))
            {
                return UI_OPEN_RESULT_DIRECTORY;
            }
            return UI_OPEN_RESULT_NONE;

        case UI_ENTRY_DIR:
            if (UI_BuildPath(g_file_browse_path, g_file_entries[g_selected_index].name, next_path, sizeof(next_path)) && UI_LoadDirectory(next_path))
            {
                return UI_OPEN_RESULT_DIRECTORY;
            }
            return UI_OPEN_RESULT_NONE;

        case UI_ENTRY_FILE:
            return UI_OPEN_RESULT_FILE;

        default:
            return UI_OPEN_RESULT_NONE;
    }
}

/**
 * @brief 根据当前目录和指定文件索引拼出完整文件路径
 * @param index 文件列表索引
 * @param out 输出路径缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=成功，0=失败
 */
uint8_t UI_BuildSelectedFilePath(uint16_t index, char *out, uint16_t out_cap)
{
    if (out == NULL || out_cap == 0U || index >= g_file_count)
    {
        return 0U;
    }

    if (g_file_entries[index].type != UI_ENTRY_FILE)
    {
        return 0U;
    }

    return UI_BuildPath(g_file_browse_path, g_file_entries[index].name, out, out_cap);
}

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
 * @brief 瑙ｇ爜鍗曚釜 UTF-8 鐮佺偣
 * @param in 杈撳叆璧峰鍦板潃
 * @param in_len 鍙敤杈撳叆闀垮害
 * @param consumed 杈撳嚭娑堣€楀瓧鑺傛暟
 * @param codepoint 杈撳嚭 Unicode 鐮佺偣
 * @retval 1=瑙ｇ爜鎴愬姛锛?=澶辫触
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
 * @brief UTF-8 瀛楄妭娴佽浆 GB2312
 * @param in UTF-8 杈撳叆缂撳啿鍖? * @param in_len 杈撳叆闀垮害
 * @param out 杈撳嚭缂撳啿鍖? * @param out_cap 杈撳嚭缂撳啿鍖哄閲? * @retval 1=鏈夋湁鏁堣緭鍑猴紝0=杞崲澶辫触
 * @details
 * 鍔熻兘锛氬皢 UTF-8 鏂囨湰杞崲涓?LCD/FatFs 鍏煎鐨?GB2312 瀛楄妭娴併€? * 璇存槑锛氭棤娉曟槧灏勭殑瀛楃缁熶竴杈撳嚭 '?'銆? */
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
 * @brief UTF-8 C 瀛楃涓茶浆 GB2312
 * @param utf8 杈撳叆 UTF-8 瀛楃涓? * @param out 杈撳嚭 GB2312 缂撳啿鍖? * @param out_cap 杈撳嚭缂撳啿鍖哄閲? * @retval 1=鎴愬姛锛?=澶辫触
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
 * @brief 鐩存帴鏄剧ず UTF-8 瀛楃涓插埌 LCD
 * @param x 璧峰 X 鍧愭爣
 * @param y 璧峰 Y 鍧愭爣
 * @param width 鏄剧ず鍖哄煙瀹藉害
 * @param height 鏄剧ず鍖哄煙楂樺害
 * @param utf8 UTF-8 瀛楃涓? * @retval 鏃? * @details
 * 娴佺▼锛歎TF-8 -> GB2312 -> LCD_ShowTextMixed銆? * 鑻ヨ浆鎹㈠け璐ワ紝鍥為€€鍒?LCD_ShowString 鏄剧ず鍘熶覆銆? */
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


