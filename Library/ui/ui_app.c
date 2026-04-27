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
 * - 璐熻矗 UI 鎬昏皟搴︿笌鎸夐敭鍒嗗彂
 * - 椤甸潰缁樺埗缁嗚妭鐢卞悇瀛愭ā鍧楁壙鎷? * ============================================================ */

/* -------------------- static 鍓嶇疆澹版槑鍖?-------------------- */
/* 鏈枃浠舵棤鍐呴儴 static 鍑芥暟 */

/**
 * @file ui_app.c
 * @brief UI 璋冨害灞傦細缁熶竴鍏ュ彛銆佸懆鏈熶换鍔°€佹寜閿矾鐢? * @details
 * 鏈枃浠朵笉鐩存帴鎵胯浇澶嶆潅椤甸潰缁嗚妭锛岃€屾槸璋冪敤鍚勫瓙妯″潡瀹屾垚鍔熻兘锛? * - 涓婚〉闈細ui_main
 * - 鍒楄〃椤甸潰锛歶i_list
 * - 鏌ョ湅椤甸潰锛歶i_view
 * - 瀛楀簱瀵煎叆锛歶i_font_import
 * - 鍒涘缓鏂囦欢锛歶i_create_file
 */

/**
 * @brief UI 鍒濆鍖? * @param 鏃? * @retval 鏃? * @details
 * 鍔熻兘锛氬垵濮嬪寲 UI 璋冨害灞傜殑鍩虹鐘舵€併€? * 杈撳叆锛氭棤銆? * 杈撳嚭锛氬埛鏂板叏灞€椤甸潰鐘舵€佷笌绉掔骇鍒锋柊鍩哄噯銆? * 娴佺▼锛? * 1. 椤甸潰鐘舵€佽涓轰富椤甸潰锛? * 2. 璁板綍褰撳墠 tick 浣滀负鏈€鍚庢搷浣滄椂闂达紱
 * 3. 閲嶇疆绉掔骇鍒锋柊缂撳瓨銆? */
void UI_Init(void)
{
    g_ui_page = UI_PAGE_MAIN;
    g_ui_last_action_tick = HAL_GetTick();
    g_last_sec = 0xFFFFFFFFUL;
}

/**
 * @brief 鍚屾 SD 鎸傝浇鐘舵€? * @param mounted SD 鐘舵€侊紙1=宸叉寕杞斤紝0=鏈寕杞斤級
 * @retval 鏃? * @details
 * 鍔熻兘锛氭妸澶栭儴瀛樺偍鐘舵€佸悓姝ョ粰 UI 閫昏緫灞傘€? * 杈撳叆锛氫笂灞傚垵濮嬪寲娴佺▼璁＄畻寰楀埌鐨勬寕杞界姸鎬併€? * 杈撳嚭锛氭洿鏂板叏灞€鍙橀噺 g_sd_mounted銆? */
void UI_SetSdMounted(uint8_t mounted)
{
    g_sd_mounted = mounted;
}

/**
 * @brief 鍚姩闃舵瀛楀簱鍒濆鍖? * @param 鏃? * @retval 鏃? * @details
 * 鍔熻兘锛氱‘淇濆瓧搴撳彲鐢ㄣ€? * 杈撳叆锛氬綋鍓?SD 鎸傝浇鐘舵€侊紙鏉ヨ嚜 g_sd_mounted锛夈€? * 杈撳嚭锛氬繀瑕佹椂瑙﹀彂瀛楀簱瀵煎叆娴佺▼銆? */
void UI_FontSystemInit(void)
{
    UI_Font_ImportSystemInit(g_sd_mounted);
}

/**
 * @brief 鏄剧ず涓婚〉闈? * @param 鏃? * @retval 鏃? * @details
 * 鍔熻兘锛氬畬鏁村埛鏂颁富椤甸潰涓讳綋涓庤繍琛屾椂闂村尯鍩熴€? * 杈撳叆锛氬綋鍓嶇郴缁熺鏁般€佹枃浠舵€绘暟绛夊叏灞€鐘舵€併€? * 杈撳嚭锛氫富椤甸潰鍐呭琚噸缁樺埌 LCD銆? * 娴佺▼锛? * 1. 缁熻鏍圭洰褰曟枃浠舵€绘暟锛? * 2. 缁樺埗涓婚〉闈富浣擄紱
 * 3. 鍒锋柊搴曢儴杩愯鏃堕棿銆? */
void UI_ShowMainPage(void)
{
    uint32_t now_sec = UI_GetCurrentSeconds();
    g_main_file_count = UI_CountRootFiles();
    UI_MainPage_Show();
    UI_MainPage_UpdateRuntime(now_sec);
}

/**
 * @brief 鎵撳嵃鎸夐敭甯姪
 * @param 鏃? * @retval 鏃? * @details
 * 鍔熻兘锛氶€氳繃涓插彛杈撳嚭涓诲姛鑳芥寜閿鏄庯紝渚夸簬璋冭瘯涓庢紨绀恒€? */
void UI_PrintHelp(void)
{
    printf("\r\n[Main] Font test case ready.\r\n");
    printf("[Main] KEY0: file list / back to main\r\n");
    printf("[Main] KEY1: color switch (main) / down (list)\r\n");
    printf("[Main] KEY2: create test file (main) / open-close file (list-view)\r\n");
    printf("[Main] KEY_UP: reload font (main) / up (list)\r\n");
}

/**
 * @brief UI 鍛ㄦ湡浠诲姟
 * @param 鏃? * @retval 鏃? * @details
 * 鍔熻兘锛氬鐞嗕笌鏃堕棿鐩稿叧鐨?UI 琛屼负銆? * 杈撳叆锛氱郴缁?tick銆佸綋鍓嶉〉闈㈢姸鎬併€? * 杈撳嚭锛氬彲鑳借Е鍙戣嚜鍔ㄥ洖涓婚〉闈㈡垨椤甸潰灞€閮ㄥ埛鏂般€? * 娴佺▼锛? * 1. 妫€鏌ラ潪涓婚〉闈㈡槸鍚﹁秴鏃讹紱
 * 2. 瓒呮椂鍒欏洖涓婚〉闈㈠苟閲嶇粯锛? * 3. 姣忕鎵ц涓€娆¤交閲忓埛鏂帮紙涓婚〉闈㈡椂閽?鍒楄〃椤靛€掕鏃讹級銆? */
void UI_Tick(void)
{
    uint32_t now_sec = UI_GetCurrentSeconds();

    /* 闈炰富椤甸潰涓嬶細30s 鏃犳搷浣滆嚜鍔ㄥ洖涓婚〉闈?*/
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

    /* 姣忕浠呭埛鏂颁竴娆★紝閬垮厤楂橀閲嶇粯 */
    if (now_sec != g_last_sec)
    {
        g_last_sec = now_sec;
        if (g_ui_page == UI_PAGE_MAIN)
        {
            /* 涓婚〉闈細鍙眬閮ㄥ埛鏂拌繍琛屾椂闂?*/
            UI_MainPage_UpdateRuntime(now_sec);
        }
        else if (g_ui_page == UI_PAGE_FILE_LIST)
        {
            /* 鍒楄〃椤碉細鍒锋柊鍊掕鏃舵垨蹇呰灞€閮ㄥ尯鍩?*/
            UI_ListPage_Show();
        }
    }
}

/**
 * @brief UI 鎸夐敭鍒嗗彂鍏ュ彛
 * @param key 褰撳墠鎸夐敭鍊? * @retval 鏃? * @details
 * 鍔熻兘锛氭牴鎹€滃綋鍓嶉〉闈?+ 鎸夐敭绫诲瀷鈥濇墽琛屽搴斿姩浣溿€? * 杈撳叆锛氭寜閿壂鎻忕粨鏋溿€? * 杈撳嚭锛氶〉闈㈢姸鎬佸彉鍖栥€丩CD 鏇存柊銆佷覆鍙ｆ棩蹇楄緭鍑恒€? * 瑙勫垯锛? * - KEY0锛氫富<->鍒楄〃/鏌ョ湅鍒囨崲锛? * - KEY1锛氫富椤甸潰鎹㈣壊锛屽垪琛ㄩ〉涓嬬Щ锛? * - KEY2锛氫富椤甸潰寤烘枃浠讹紝鍒楄〃椤垫墦寮€锛屾煡鐪嬮〉鍏抽棴锛? * - KEY_UP锛氫富椤甸潰瀵煎瓧搴擄紝鍒楄〃椤典笂绉汇€? */
void UI_HandleKey(KeyName_t key)
{
    uint32_t now_sec = UI_GetCurrentSeconds();
    if (key == KEY_NONE)
    {
        return;
    }

    /* 任意有效按键都会刷新最后操作时间，用于自动返回主页面计时。 */
    g_ui_last_action_tick = HAL_GetTick();

    switch (key)
    {
        case KEY0:
        {
            /* 查看页下按 KEY0：关闭当前查看页并回到当前目录列表。
             * 这里不能直接回主页面，否则用户会丢失自己当前所在的目录层级。 */
            if (g_ui_page == UI_PAGE_FILE_VIEW)
            {
                g_ui_page = UI_PAGE_FILE_LIST;
                g_file_list_need_full_redraw = 1U;
                UI_ListPage_Show();
                printf("[KEY0] Back to current file list\r\n");
                break;
            }

            /* 列表页下按 KEY0：统一返回主页面。
             * 父目录返回动作不再由 KEY0 承担，而是通过选中 `[..]` 后按 KEY2 完成。 */
            if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                g_ui_page = UI_PAGE_MAIN;
                g_main_file_count = UI_CountRootFiles();
                UI_MainPage_Show();
                UI_MainPage_UpdateRuntime(now_sec);
                printf("[KEY0] Back to main page\r\n");
                break;
            }

            /* 主页面下按 KEY0：从根目录进入通用文件浏览器。 */
            (void)UI_LoadRootFileList();
            UI_EnterPage(UI_PAGE_FILE_LIST);
            UI_ListPage_Show();
            printf("[KEY0] Enter file browser, count=%u\r\n", (unsigned int)g_file_count);
            break;
        }

        case KEY1:
        {
            if (g_ui_page == UI_PAGE_MAIN)
            {
                /* 主页面 KEY1：保留原有文字颜色切换功能。 */
                g_test_fg_mode = (uint8_t)((g_test_fg_mode + 1U) % 3U);
                UI_MainPage_Show();
                UI_MainPage_UpdateRuntime(now_sec);
                printf("[KEY1] Switch text color mode: %u\r\n", g_test_fg_mode);
            }
            else if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                /* 列表页 KEY1：向下移动选中项。 */
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
                /* 主页面 KEY2：保留原有创建测试文件功能。 */
                UI_CreateTestFileAndShow();
                UI_ShowMainPage();
            }
            else if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                /* 列表页 KEY2：统一处理“父目录 / 目录 / 文件”三类条目。
                 * - 父目录或目录：切换目录并刷新列表页
                 * - 文件：进入查看页 */
                UiOpenResult_t open_result = UI_OpenSelectedEntry();
                if (open_result == UI_OPEN_RESULT_DIRECTORY)
                {
                    g_file_list_need_full_redraw = 1U;
                    UI_ListPage_Show();
                    printf("[KEY2] Enter directory: %s\r\n", UI_GetCurrentDir());
                }
                else if (open_result == UI_OPEN_RESULT_FILE)
                {
                    UI_View_OpenSelected();
                }
            }
            else if (g_ui_page == UI_PAGE_FILE_VIEW)
            {
                /* 查看页 KEY2：保留原有关闭查看页返回列表的行为。
                 * 这样查看页同时支持 KEY0 和 KEY2 两个返回键。 */
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
                /* 主页面 KEY_UP：保留原有字库重新导入功能。 */
                UI_Font_RunImportFromMain();
                g_main_file_count = UI_CountRootFiles();
                UI_MainPage_Show();
                UI_MainPage_UpdateRuntime(now_sec);
            }
            else if (g_ui_page == UI_PAGE_FILE_LIST)
            {
                /* 列表页 KEY_UP：向上移动选中项。 */
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
