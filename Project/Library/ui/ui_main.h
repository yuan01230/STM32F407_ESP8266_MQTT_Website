#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <stdint.h>

/**
 * @brief 绘制主页面主体内容
 * @details
 * 包含中英文混排演示、按键提示、状态信息等区域。
 */
void UI_MainPage_Show(void);

/**
 * @brief 刷新主页面运行时间显示
 * @param seconds 系统运行秒数
 */
void UI_MainPage_UpdateRuntime(uint32_t seconds);

#endif
