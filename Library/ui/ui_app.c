#include "ui_app.h"

#include <stdio.h>

#include "../../Core/Inc/main.h"
#include "ui_common.h"
#include "ui_main.h"
#include "ui_list.h"
#include "ui_view.h"
#include "ui_font_import.h"
#include "ui_create_file.h"

/* ============================================================
 * UI Dispatch Layer (ui_app.c)
 * - 负责 UI 总调度与按键分发
 * - 页面绘制细节由各子模块承担
 * ============================================================ */

/* -------------------- static 前置声明区 -------------------- */
/* 本文件无内部 static 函数 */

/**
 * @file ui_app.c
 * @brief UI 调度层：统一入口、周期任务、按键路由
 * @details
 * 本文件不直接承载复杂页面细节，而是调用各子模块完成功能：
 * - 主页面：ui_main
 * - 列表页面：ui_list
 * - 查看页面：ui_view
 * - 字库导入：ui_font_import
 * - 创建文件：ui_create_file
 */

/**
 * @brief UI 初始化
 * @param 无
 * @retval 无
 * @details
 * 功能：初始化 UI 调度层的基础状态。
 * 输入：无。
 * 输出：刷新全局页面状态与秒级刷新基准。
 * 流程：
 * 1. 页面状态设为主页面；
 * 2. 记录当前 tick 作为最后操作时间；
 * 3. 重置秒级刷新缓存。
 */
void UI_Init(void)
{
    g_ui_page = UI_PAGE_MAIN;
    g_ui_last_action_tick = HAL_GetTick();
    g_last_sec = 0xFFFFFFFFUL;
}

/**
 * @brief 同步 SD 挂载状态
 * @param mounted SD 状态（1=已挂载，0=未挂载）
 * @retval 无
 * @details
 * 功能：把外部存储状态同步给 UI 逻辑层。
 * 输入：上层初始化流程计算得到的挂载状态。
 * 输出：更新全局变量 g_sd_mounted。
 */
void UI_SetSdMounted(uint8_t mounted)
{
    g_sd_mounted = mounted;
}

/**
 * @brief 启动阶段字库初始化
 * @param 无
 * @retval 无
 * @details
 * 功能：确保字库可用。
 * 输入：当前 SD 挂载状态（来自 g_sd_mounted）。
 * 输出：必要时触发字库导入流程。
 */
void UI_FontSystemInit(void)
{
    UI_Font_ImportSystemInit(g_sd_mounted);
}

/**
 * @brief 显示主页面
 * @param 无
 * @retval 无
 * @details
 * 功能：完整刷新主页面主体与运行时间区域。
 * 输入：当前系统秒数、文件总数等全局状态。
 * 输出：主页面内容被重绘到 LCD。
 * 流程：
 * 1. 统计根目录文件总数；
 * 2. 绘制主页面主体；
 * 3. 刷新底部运行时间。
 */
void UI_ShowMainPage(void)
{
    uint32_t now_sec = UI_GetCurrentSeconds();
    g_main_file_count = UI_CountRootFiles();
    UI_MainPage_Show();
    UI_MainPage_UpdateRuntime(now_sec);
}

/**
 * @brief 打印按键帮助
 * @param 无
 * @retval 无
 * @details
 * 功能：通过串口输出主功能按键说明，便于调试与演示。
 */
void UI_PrintHelp(void)
{
    printf("\r\n[Main] Font test case ready.\r\n");
    printf("[Main] KEY0: file list / back to main\r\n");
    printf("[Main] KEY1: color switch (main) / down (list)\r\n");
    printf("[Main] KEY2: create test file (main) / open-close file (list-view)\r\n");
    printf("[Main] KEY_UP: reload font (main) / up (list)\r\n");
}

/**
 * @brief UI 周期任务
 * @param 无
 * @retval 无
 * @details
 * 功能：处理与时间相关的 UI 行为。
 * 输入：系统 tick、当前页面状态。
 * 输出：可能触发自动回主页面或页面局部刷新。
 * 流程：
 * 1. 检查非主页面是否超时；
 * 2. 超时则回主页面并重绘；
 * 3. 每秒执行一次轻量刷新（主页面时钟/列表页倒计时）。
 */
void UI_Tick(void)
{
    uint32_t now_sec = UI_GetCurrentSeconds();

    /* 非主页面下：30s 无操作自动回主页面 */
    if (g_ui_page != UI_PAGE_MAIN)
    {
        if ((HAL_GetTick() - g_ui_last_action_tick) >= UI_AUTO_RETURN_MS)
        {
            g_ui_page = UI_PAGE_MAIN;
            g_main_file_count = UI_CountRootFiles();
            UI_MainPage_Show();
            UI_MainPage_UpdateRuntime(now_sec);
            printf("[UI] Auto return to main page\r\n");
        }
    }

    /* 每秒仅刷新一次，避免高频重绘 */
    if (now_sec != g_last_sec)
    {
        g_last_sec = now_sec;
        if (g_ui_page == UI_PAGE_MAIN)
        {
            /* 主页面：只局部刷新运行时间 */
            UI_MainPage_UpdateRuntime(now_sec);
        }
        else if (g_ui_page == UI_PAGE_FILE_LIST)
        {
            /* 列表页：刷新倒计时或必要局部区域 */
            UI_ListPage_Show();
        }
    }
}

/**
 * @brief UI 按键分发入口
 * @param key 当前按键值
 * @retval 无
 * @details
 * 功能：根据“当前页面 + 按键类型”执行对应动作。
 * 输入：按键扫描结果。
 * 输出：页面状态变化、LCD 更新、串口日志输出。
 * 规则：
 * - KEY0：主<->列表/查看切换；
 * - KEY1：主页面换色，列表页下移；
 * - KEY2：主页面建文件，列表页打开，查看页关闭；
 * - KEY_UP：主页面导字库，列表页上移。
 */
void UI_HandleKey(KeyName_t key)
{
    uint32_t now_sec = UI_GetCurrentSeconds();
    if (key == KEY_NONE) return;

    /* 任意有效按键都重置“无操作自动返回”计时 */
    g_ui_last_action_tick = HAL_GetTick();

    /* 按“当前页面 + 按键值”路由到对应功能 */
    switch (key)
    {
        case KEY0:
        {
            /* 列表页/查看页：统一返回主页面 */
            if (g_ui_page == UI_PAGE_FILE_VIEW || g_ui_page == UI_PAGE_FILE_LIST)
            {
                g_ui_page = UI_PAGE_MAIN;
                g_main_file_count = UI_CountRootFiles();
                UI_MainPage_Show();
                UI_MainPage_UpdateRuntime(now_sec);
                printf("[KEY0] Back to main page\r\n");
                break;
            }

            /* 主页面 KEY0：进入列表页
             * 步骤：加载文件缓存 -> 切页 -> 绘制列表
             */
            (void)UI_LoadRootFileList();
            UI_EnterPage(UI_PAGE_FILE_LIST);
            UI_ListPage_Show();
            printf("[KEY0] Enter file list page, count=%u\r\n", (unsigned int)g_file_count);
            break;
        }

        case KEY1:
        {
            if (g_ui_page == UI_PAGE_MAIN)
            {
                /* 主页面 KEY1：切换文本颜色模式并重绘主页面 */
                g_test_fg_mode = (uint8_t)((g_test_fg_mode + 1U) % 3U);
                UI_MainPage_Show();
                UI_MainPage_UpdateRuntime(now_sec);
                printf("[KEY1] Switch text color mode: %u\r\n", g_test_fg_mode);
            }
            else if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                /* 列表页 KEY1：选中项向下循环移动 */
                UI_List_SelectDown();
                UI_ListPage_Show();
                printf("[KEY1] Select down: %u\r\n", (unsigned int)g_selected_index);
            }
            break;
        }

        case KEY2:
        {
            if (g_ui_page == UI_PAGE_MAIN)
            {
                /* 主页面 KEY2：创建文件 -> 显示结果页 -> 回主页面 */
                UI_CreateTestFileAndShow();
                UI_ShowMainPage();
            }
            else if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                /* 列表页 KEY2：打开当前选中项进入查看页 */
                UI_View_OpenSelected();
            }
            else if (g_ui_page == UI_PAGE_FILE_VIEW)
            {
                /* 查看页 KEY2：关闭查看并返回列表页 */
                g_ui_page = UI_PAGE_FILE_LIST;
                g_file_list_need_full_redraw = 1U;
                UI_ListPage_Show();
                printf("[KEY2] Back to file list page\r\n");
            }
            break;
        }

        case KEY_UP:
        {
            if (g_ui_page == UI_PAGE_MAIN)
            {
                /* 主页面 KEY_UP：重新导入字库并返回主页面 */
                UI_Font_RunImportFromMain();
                g_main_file_count = UI_CountRootFiles();
                UI_MainPage_Show();
                UI_MainPage_UpdateRuntime(now_sec);
            }
            else if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                /* 列表页 KEY_UP：选中项向上循环移动 */
                UI_List_SelectUp();
                UI_ListPage_Show();
                printf("[KEY_UP] Select up: %u\r\n", (unsigned int)g_selected_index);
            }
            break;
        }

        default:
            break;
    }
}
