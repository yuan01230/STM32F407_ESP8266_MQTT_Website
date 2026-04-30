#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <stdint.h>

#include "../key/key.h"
#include "ff.h"

/**
 * @brief UI 页面枚举
 * @details
 * 当前工程的 UI 被拆成三个稳定页面：
 * 1. 主页面：展示状态信息和按键入口；
 * 2. 文件列表页：显示当前目录下的目录项和文件项；
 * 3. 文件查看页：显示普通文件内容或图片预览结果。
 */
typedef enum
{
    UI_PAGE_MAIN = 0,
    UI_PAGE_FILE_LIST,
    UI_PAGE_FILE_VIEW
} UiPage_t;

/**
 * @brief 文件浏览器条目类型
 * @details
 * 为了让列表页和按键层知道“当前选中的是什么”，
 * 每个条目都需要明确区分：
 * - `[..]` 虚拟父目录项
 * - FATFS 真实目录项
 * - FATFS 普通文件项
 */
typedef enum
{
    UI_ENTRY_PARENT = 0,
    UI_ENTRY_DIR,
    UI_ENTRY_FILE
} UiEntryType_t;

/**
 * @brief 打开列表项后的结果类型
 * @details
 * 在列表页按下 `KEY2` 后，当前选中项可能触发三种结果：
 * 1. 没有产生有效动作，例如索引越界；
 * 2. 目录切换成功，需要刷新当前列表页；
 * 3. 选中的是普通文件，需要进入查看页。
 */
typedef enum
{
    UI_OPEN_RESULT_NONE = 0,
    UI_OPEN_RESULT_DIRECTORY,
    UI_OPEN_RESULT_FILE
} UiOpenResult_t;

/**
 * @brief 文件浏览器条目信息
 * @details
 * 当前结构同时服务于目录浏览和普通文件查看：
 * - `name` 保存显示名称；
 * - `ext` 保存扩展名，目录项可为空；
 * - `size` 仅对普通文件有实际意义；
 * - `type` 用于决定 `KEY2` 的动作。
 */
typedef struct
{
    char name[64];
    char ext[16];
    uint32_t size;
    uint8_t is_dir;
    UiEntryType_t type;
} FileEntry_t;

/** SD 卡字库文件默认路径。 */
#define FONT_SD_PATH "0:/Font/HZK16"
/** 测试文件默认路径，使用 UTF-8 文件名。 */
#define TEST_FILE_PATH_UTF8 "0:/测试专用.txt"
/** 文件浏览器根目录。 */
#define UI_ROOT_DIR_PATH "0:/"
/** 非主页面无操作自动返回超时，单位毫秒。 */
#define UI_AUTO_RETURN_MS 30000UL
/** 文件列表最大缓存条目数。 */
#define MAX_FILE_ENTRIES 64U
/** 列表页单页可见行数。 */
#define FILE_LIST_PAGE_ROWS 11U
/** 文件查看页最大读取字节数。 */
#define FILE_VIEW_READ_MAX 320U
/** 内部通用文件路径缓冲区长度。 */
#define UI_FILE_PATH_MAX 80U

/** SD 卡是否已成功挂载。 */
extern uint8_t g_sd_mounted;
/** 主页面测试字体颜色模式。 */
extern uint8_t g_test_fg_mode;
/** 当前 UI 所在页面。 */
extern UiPage_t g_ui_page;
/** 最近一次按键时间戳，用于自动返回计时。 */
extern uint32_t g_ui_last_action_tick;
/** 主页面上显示的根目录条目数量。 */
extern uint16_t g_main_file_count;
/** UI 秒级刷新去抖缓存。 */
extern uint32_t g_last_sec;
/** 文件列表缓存。 */
extern FileEntry_t g_file_entries[MAX_FILE_ENTRIES];
/** 当前浏览目录，例如 `0:/`、`0:/Picture`。 */
extern char g_file_browse_path[UI_FILE_PATH_MAX];
/** 当前目录已加载条目数。 */
extern uint16_t g_file_count;
/** 当前高亮选中条目索引。 */
extern uint16_t g_selected_index;
/** 当前页面显示窗口顶部条目索引。 */
extern uint16_t g_list_top_index;
/** 当前查看页对应的文件索引。 */
extern uint16_t g_view_index;
/** 文件列表页是否需要整页重绘。 */
extern uint8_t g_file_list_need_full_redraw;

/**
 * @brief 进入指定页面并刷新无操作计时
 * @param page 目标页面
 */
void UI_EnterPage(UiPage_t page);

/**
 * @brief 获取系统运行秒数
 * @return 当前运行时间，单位秒
 */
uint32_t UI_GetCurrentSeconds(void);

/**
 * @brief 获取自动返回剩余秒数
 * @return 剩余秒数；为 0 表示已经超时
 */
uint32_t UI_GetRemainSeconds(void);

/**
 * @brief 统计根目录下的可见条目数量
 * @return 条目数量
 * @details
 * 为兼容主页面的旧调用点，保留“RootFiles”命名。
 * 实际统计对象包含根目录下的目录和普通文件，不包含 `[..]`。
 */
uint16_t UI_CountRootFiles(void);

/**
 * @brief 加载根目录列表到缓存
 * @return 加载后的条目数量
 * @details
 * 该接口会把浏览器当前目录重置为 `0:/`，再加载根目录内容。
 */
uint16_t UI_LoadRootFileList(void);

/**
 * @brief 加载指定目录到文件列表缓存
 * @param path 目标目录路径
 * @return 1 表示成功，0 表示失败
 * @details
 * 成功后会同步更新：
 * - 当前浏览路径
 * - 列表缓存
 * - 当前选中索引
 * - 列表页窗口位置
 */
uint8_t UI_LoadDirectory(const char *path);

/**
 * @brief 获取当前浏览目录
 * @return 当前目录字符串
 */
const char *UI_GetCurrentDir(void);

/**
 * @brief 判断当前目录是否为根目录
 * @return 1 表示根目录，0 表示非根目录
 */
uint8_t UI_IsRootDir(void);

/**
 * @brief 打开当前选中条目
 * @return UiOpenResult_t 打开结果
 * @details
 * 调用方根据返回值决定是刷新列表页，还是进入查看页。
 */
UiOpenResult_t UI_OpenSelectedEntry(void);

/**
 * @brief 生成当前选中文件的完整路径
 * @param index 文件索引
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @return 1 表示成功，0 表示失败
 * @details
 * 只有普通文件条目允许生成文件路径；
 * `[..]` 和目录项调用本接口会直接失败。
 */
uint8_t UI_BuildSelectedFilePath(uint16_t index, char *out, uint16_t out_cap);

/**
 * @brief 判断一段缓冲区是否为合法 UTF-8 文本
 * @param buf 输入缓冲区
 * @param len 输入长度
 * @return 1 表示合法，0 表示非法
 */
uint8_t UI_IsValidUtf8(const uint8_t *buf, uint16_t len);

/**
 * @brief 解码单个 UTF-8 码点
 * @param in 输入字节流
 * @param in_len 可用输入长度
 * @param consumed 输出消耗字节数
 * @param codepoint 输出 Unicode 码点
 * @return 1 表示成功，0 表示失败
 */
uint8_t UI_Utf8DecodeCodepoint(const uint8_t *in, uint16_t in_len, uint16_t *consumed, uint32_t *codepoint);

/**
 * @brief 将 UTF-8 文本转换为 GB2312 字节流
 * @param in UTF-8 输入
 * @param in_len 输入长度
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @return 1 表示成功，0 表示失败
 */
uint8_t UI_Utf8ToGb2312(const uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t out_cap);

/**
 * @brief 将 UTF-8 字符串转换为 GB2312 字符串
 * @param utf8 UTF-8 输入字符串
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @return 1 表示成功，0 表示失败
 */
uint8_t UI_TextUtf8ToGb2312(const char *utf8, uint8_t *out, uint16_t out_cap);

/**
 * @brief 直接在 LCD 指定区域显示 UTF-8 文本
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param width 可显示宽度
 * @param height 可显示高度
 * @param utf8 UTF-8 输入字符串
 * @details
 * 内部会先把 UTF-8 转换为当前工程使用的 GB2312，再交给 LCD 显示层。
 */
void UI_LCD_ShowUtf8(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *utf8);

#endif

