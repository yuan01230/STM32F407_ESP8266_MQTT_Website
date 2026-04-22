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
 * @brief 文件项信息结构
 */
typedef struct
{
    /** 文件名（含扩展名） */
    char name[64];
    /** 扩展名（不含点） */
    char ext[16];
    /** 文件大小（字节） */
    uint32_t size;
    /** 是否目录：1=目录，0=普通文件 */
    uint8_t is_dir;
} FileEntry_t;

/** SD 卡字库路径 */
#define FONT_SD_PATH "0:/Font/HZK16"
/** KEY2 创建测试文件的 UTF-8 路径 */
#define TEST_FILE_PATH_UTF8 "0:/测试专用.txt"
/** 非主页面无操作自动返回主页面的超时（ms） */
#define UI_AUTO_RETURN_MS 30000UL
/** 列表最大缓存文件数 */
#define MAX_FILE_ENTRIES 64U
/** 列表页单页显示行数 */
#define FILE_LIST_PAGE_ROWS 11U
/** 文件查看页最大读取字节数 */
#define FILE_VIEW_READ_MAX 320U

/** SD 挂载状态（由主流程同步） */
extern uint8_t g_sd_mounted;
/** 主页面字体颜色模式（0黑/1蓝/2红） */
extern uint8_t g_test_fg_mode;
/** 当前 UI 页面 */
extern UiPage_t g_ui_page;
/** 最近一次按键时间戳（HAL_GetTick） */
extern uint32_t g_ui_last_action_tick;
/** 主页面显示的 SD 根目录文件总数 */
extern uint16_t g_main_file_count;
/** UI 秒级刷新去抖：记录上一次秒值 */
extern uint32_t g_last_sec;
/** 文件列表缓存数组 */
extern FileEntry_t g_file_entries[MAX_FILE_ENTRIES];
/** 当前已加载的文件数量 */
extern uint16_t g_file_count;
/** 当前选中的文件索引 */
extern uint16_t g_selected_index;
/** 当前列表页顶部索引（分页窗口起点） */
extern uint16_t g_list_top_index;
/** 当前查看页对应的文件索引 */
extern uint16_t g_view_index;
/** 列表页是否需要整页重绘 */
extern uint8_t g_file_list_need_full_redraw;

/**
 * @brief 进入指定页面并刷新无操作计时
 * @param page 目标页面
 */
void UI_EnterPage(UiPage_t page);

/**
 * @brief 获取系统运行秒数
 * @retval 秒值（HAL_GetTick/1000）
 */
uint32_t UI_GetCurrentSeconds(void);

/**
 * @brief 获取自动返回剩余秒数
 * @retval 剩余秒数（0 表示已超时）
 */
uint32_t UI_GetRemainSeconds(void);

/**
 * @brief 统计 SD 根目录普通文件数量
 * @retval 文件数量
 */
uint16_t UI_CountRootFiles(void);

/**
 * @brief 加载 SD 根目录普通文件到缓存列表
 * @retval 加载后的文件总数
 */
uint16_t UI_LoadRootFileList(void);

/**
 * @brief 判断缓冲区是否为有效 UTF-8 文本
 * @param buf 输入缓冲区
 * @param len 输入长度
 * @retval 1=是 UTF-8 文本，0=否
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
 * @brief UTF-8 文本转换为 GB2312 编码字节流
 * @param in UTF-8 输入
 * @param in_len 输入长度
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=转换得到输出，0=未得到有效输出
 */
uint8_t UI_Utf8ToGb2312(const uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t out_cap);

/**
 * @brief UTF-8 字符串转 GB2312 字符串（以 '\0' 结尾）
 * @param utf8 UTF-8 输入字符串
 * @param out 输出缓冲区
 * @param out_cap 输出缓冲区容量
 * @retval 1=转换成功，0=失败
 */
uint8_t UI_TextUtf8ToGb2312(const char *utf8, uint8_t *out, uint16_t out_cap);

/**
 * @brief 以 UTF-8 文本直接显示到 LCD（内部自动转 GB2312）
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param width 显示宽度
 * @param height 显示高度
 * @param utf8 UTF-8 文本
 */
void UI_LCD_ShowUtf8(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *utf8);

#endif
