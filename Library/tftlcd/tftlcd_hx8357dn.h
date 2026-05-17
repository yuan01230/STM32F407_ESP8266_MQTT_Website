#ifndef TFTLCD_HX8357DN_H
#define TFTLCD_HX8357DN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _tftlcd_data;

/**
 * @brief 向 HX8357DN 写入命令
 * @param cmd 16 位命令值
 */
void HX8357DN_WriteCmd(uint16_t cmd);

/**
 * @brief 向 HX8357DN 写入数据
 * @param data 16 位数据值
 */
void HX8357DN_WriteData(uint16_t data);

/**
 * @brief 从 HX8357DN 读取数据
 * @return 读取到的 16 位数据
 */
uint16_t HX8357DN_ReadData(void);

/**
 * @brief 向 HX8357DN 连续写入一个 RGB565 像素
 * @param color RGB565 颜色值
 */
void HX8357DN_WriteColor(uint16_t color);

/**
 * @brief 设置 HX8357DN 显示方向
 * @param runtime LCD 运行时参数对象
 * @param dir `0` 竖屏，`1` 横屏
 */
void HX8357DN_SetDisplayDir(struct _tftlcd_data *runtime, uint8_t dir);

/**
 * @brief 设置 HX8357DN 显示窗口
 * @param sx 起始 X 坐标
 * @param sy 起始 Y 坐标
 * @param ex 结束 X 坐标
 * @param ey 结束 Y 坐标
 */
void HX8357DN_SetWindow(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey);

/**
 * @brief 读取 HX8357DN 控制器 ID
 * @return 读出的控制器 ID
 */
uint16_t HX8357DN_ReadId(void);

/**
 * @brief 初始化 HX8357DN 控制器
 * @param runtime LCD 运行时参数对象
 */
void HX8357DN_Init(struct _tftlcd_data *runtime);

/**
 * @brief 读取指定像素点颜色
 * @param x X 坐标
 * @param y Y 坐标
 * @return RGB565 颜色值
 */
uint16_t HX8357DN_ReadPoint(uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif
