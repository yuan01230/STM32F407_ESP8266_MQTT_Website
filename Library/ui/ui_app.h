#ifndef UI_APP_H
#define UI_APP_H

#include <stdint.h>
#include "../key/key.h"

/**
 * @brief UI 模块总入口初始化
 * @details
 * 初始化 UI 页面状态与计时基准，通常在外设初始化后调用一次。
 */
void UI_Init(void);

/**
 * @brief 同步 SD 挂载状态到 UI 模块
 * @param mounted 1=已挂载，0=未挂载
 */
void UI_SetSdMounted(uint8_t mounted);

/**
 * @brief UI 层字库系统初始化
 * @details
 * 启动时检查外部 Flash 字库，必要时尝试从 SD 导入。
 */
void UI_FontSystemInit(void);

/**
 * @brief 显示主页面并刷新主页面运行时
 */
void UI_ShowMainPage(void);

/**
 * @brief 打印主页面按键帮助信息到串口
 */
void UI_PrintHelp(void);

/**
 * @brief UI 周期任务（建议在主循环中每次调用）
 * @details
 * 处理页面超时自动返回、秒级倒计时/运行时刷新。
 */
void UI_Tick(void);

/**
 * @brief UI 按键事件入口
 * @param key 当前扫描到的按键值
 * @details
 * 根据当前页面与按键类型进行路由分发。
 */
void UI_HandleKey(KeyName_t key);

#endif
