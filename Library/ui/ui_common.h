#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <stdint.h>
#include "../key/key.h"
#include "ff.h"

/**
 * @brief UI 页面枚举
 */
typedef enum
{
    /** 主页面 */
    UI_PAGE_MAIN = 0,
    /** 文件列表页面 */
    UI_PAGE_FILE_LIST,
    /** 文件查看页面 */
    UI_PAGE_FILE_VIEW
} UiPage_t;

/**
 * @brief 文件列表项类型
 * @details
 * 当前文件浏览器中的每一项都必须明确自己的类型，
 * 这样列表页才能决定如何显示，按键处理层才能决定 KEY2 的行为。
 */
typedef enum
{
    /** 虚拟父目录项，显示为 [..]，不来自 FATFS。 */
    UI_ENTRY_PARENT = 0,
    /** 普通目录项，来自 FATFS。 */
    UI_ENTRY_DIR,
    /** 普通文件项，来自 FATFS。 */
    UI_ENTRY_FILE
} UiEntryType_t;

/**
 * @brief 打开当前选中项后的结果类型
 * @details
 * 列表页按下 KEY2 后，可能发生三种结果：
 * 1. 什么都没发生，例如索引无效；
 * 2. 目录已经切换成功，此时应刷新列表页；
 * 3. 当前项是文件，此时应进入查看页。
 */
typedef enum
{
    /** 当前操作没有产生有效结果。 */
    UI_OPEN_RESULT_NONE = 0,
    /** 已经完成目录切换，调用方应刷新列表页。 */
    UI_OPEN_RESULT_DIRECTORY,
    /** 当前项是文件，调用方应进入查看页。 */
    UI_OPEN_RESULT_FILE
} UiOpenResult_t;

/**
 * @brief 文件项信息结构
 */
typedef struct
{
    /** 文件名或目录名；父目录项固定为 ".."。 */
    char name[64];
    /** 文件扩展名，不包含点号；目录项和父目录项可为空。 */
    char ext[16];
    /** 文件大小，目录项和父目录项通常为 0。 */
    uint32_t size;
    /** 是否目录，1 表示目录，0 表示普通文件。 */
    uint8_t is_dir;
    /** 当前条目的类型，用于区分父项 / 目录 / 文件。 */
    UiEntryType_t type;
} FileEntry_t;

/** SD 卡字库路径 */
#define FONT_SD_PATH "0:/Font/HZK16"
/** KEY2 创建测试文件的 UTF-8 路径 */
#define TEST_FILE_PATH_UTF8 "0:/测试专用.txt"
/** 浏览器根目录路径 */
#define UI_ROOT_DIR_PATH "0:/"
/** 非主页面无操作自动返回主页面的超时，单位 ms */
#define UI_AUTO_RETURN_MS 30000UL
/** 列表最大缓存文件数 */
#define MAX_FILE_ENTRIES 64U
/** 列表页单页显示行数 */
#define FILE_LIST_PAGE_ROWS 11U
/** 文件查看页最大读取字节数 */
#define FILE_VIEW_READ_MAX 320U
/** UI 内部统一使用的文件路径缓冲区长度 */
#define UI_FILE_PATH_MAX 80U

/** SD 挂载状态，由主流程同步 */
extern uint8_t g_sd_mounted;
/** 主页面字体颜色模式 */
extern uint8_t g_test_fg_mode;
/** 当前 UI 页面 */
extern UiPage_t g_ui_page;
/** 最近一次按键时间戳 */
extern uint32_t g_ui_last_action_tick;
/** 主页面显示的根目录条目总数 */
extern uint16_t g_main_file_count;
/** UI 秒级刷新去抖缓存 */
extern uint32_t g_last_sec;
/** 文件列表缓存数组 */
extern FileEntry_t g_file_entries[MAX_FILE_ENTRIES];
/** 当前浏览器所在目录，例如 0:/、0:/Picture、0:/Picture/Sub */
extern char g_file_browse_path[UI_FILE_PATH_MAX];
/** 当前已加载的列表项数量 */
extern uint16_t g_file_count;
/** 当前选中的文件索引 */
extern uint16_t g_selected_index;
/** 当前列表页顶部索引 */
extern uint16_t g_list_top_index;
/** 当前查看页对应的文件索引 */
extern uint16_t g_view_index;
/** 文件列表页是否需要整页重绘 */
extern uint8_t g_file_list_need_full_redraw;

/**
 * @brief 进入指定页面并刷新无操作计时
 * @param page 目标页面
 */
void UI_EnterPage(UiPage_t page);

/**
 * @brief 获取系统运行秒数
 * @retval 当前运行秒数
 */
uint32_t UI_GetCurrentSeconds(void);

/**
 * @brief 获取自动返回剩余秒数
 * @retval 剩余秒数，0 表示已超时
 */
uint32_t UI_GetRemainSeconds(void);

/**
 * @brief 统计根目录中的可见条目数量
 * @retval 条目数量
 * @details
 * 该接口保留原函数名，以兼容主页面现有调用点。
 * 统计对象包括根目录下的目录和普通文件，但不包含虚拟父目录项。
 */
uint16_t UI_CountRootFiles(void);

/**
 * @brief 加载根目录列表到缓存
 * @retval 加载后的条目数量
 * @details
 * 该接口保留原函数名，以兼容主页面进入文件列表的现有调用点。
 * 实际实现会调用通用目录加载函数并把当前目录切换为 0:/。
 */
uint16_t UI_LoadRootFileList(void);

/**
 * @brief 加载指定目录到文件列表缓存
 * @param path 目标目录路径
 * @retval 1=成功，0=失败
 * @details
 * 该函数是通用文件浏览器的核心入口。
 * 成功后会同步更新：
 * 1. 当前目录路径；
 * 2. 列表条目缓存；
 * 3. 选中索引与分页窗口。
 */
uint8_t UI_LoadDirectory(const char *path);

/**
 * @brief 获取当前浏览器目录
 * @retval 当前目录路径
 */
const char *UI_GetCurrentDir(void);

/**
 * @brief 判断当前目录是否为根目录
 * @retval 1=当前就是根目录，0=不是根目录
 */
uint8_t UI_IsRootDir(void);

/**
 * @brief 打开当前选中条目
 * @retval UiOpenResult_t 打开结果
 * @details
 * 该接口只负责目录切换决策，不直接进入查看页。
 * 调用方根据返回值决定是刷新列表页还是切到查看页。
 */
UiOpenResult_t UI_OpenSelectedEntry(void);

/**
 * @brief 根据当前目录和指定文件索引拼出完整文件路径
 * @param index 文件列表索引
 * @param out 输出路径缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=成功，0=失败
 * @details
 * 只有普通文件项才允许生成文件完整路径。
 * 父目录项和目录项调用该接口时会直接失败。
 */
uint8_t UI_BuildSelectedFilePath(uint16_t index, char *out, uint16_t out_cap);

/**
 * @brief 判断缓冲区是否为有效 UTF-8 文本
 * @param buf 输入缓冲区
 * @param len 输入长度
 * @retval 1=是有效 UTF-8 文本，0=否
 */
uint8_t UI_IsValidUtf8(const uint8_t *buf, uint16_t len);

/**
 * @brief 解码单个 UTF-8 码点
 * @param in 输入字节流
 * @param in_len 输入剩余长度
 * @param consumed 输出消耗字节数
 * @param codepoint 输出 Unicode 码点
 * @retval 1=成功，0=失败
 */
uint8_t UI_Utf8DecodeCodepoint(const uint8_t *in, uint16_t in_len, uint16_t *consumed, uint32_t *codepoint);

/**
 * @brief 将 UTF-8 文本转换为 GB2312 字节流
 * @param in UTF-8 输入
 * @param in_len 输入长度
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=成功得到输出，0=失败
 */
uint8_t UI_Utf8ToGb2312(const uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t out_cap);

/**
 * @brief 将 UTF-8 字符串转换为 GB2312 字符串
 * @param utf8 UTF-8 输入字符串
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=转换成功，0=失败
 */
uint8_t UI_TextUtf8ToGb2312(const char *utf8, uint8_t *out, uint16_t out_cap);

/**
 * @brief 将 UTF-8 文本直接显示到 LCD，内部自动转 GB2312
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param width 显示宽度
 * @param height 显示高度
 * @param utf8 UTF-8 文本
 */
void UI_LCD_ShowUtf8(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *utf8);

#endif
