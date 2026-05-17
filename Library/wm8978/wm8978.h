#ifndef WM8978_H
#define WM8978_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WM8978_I2C_ADDR_7BIT        0x1AU
#define WM8978_DEFAULT_DAC_VOLUME   255U
#define WM8978_IGNORE_ACK_TEST      0U

/**
 * @brief 初始化 WM8978 到播放状态。
 * @param 无。
 * @return 0 表示成功；非 0 表示地址探测或寄存器写入失败。
 */
uint8_t WM8978_Init(void);

/**
 * @brief 写 WM8978 9bit 寄存器。
 * @param reg 寄存器编号。
 * @param value 9bit 寄存器值，函数内部只使用低 9bit。
 * @return 0 表示成功；1/2/3/4 表示不同错误阶段。
 */
uint8_t WM8978_WriteReg(uint8_t reg, uint16_t value);

/**
 * @brief 设置 DAC 数字音量。
 * @param volume DAC 音量，范围 0 到 255。
 * @return 0 表示成功；非 0 表示寄存器写入失败。
 */
uint8_t WM8978_SetDACVolume(uint8_t volume);

/**
 * @brief 设置耳机左右声道音量。
 * @param left 左声道音量，范围 0 到 63。
 * @param right 右声道音量，范围 0 到 63。
 * @return 0 表示成功；非 0 表示寄存器写入失败。
 */
uint8_t WM8978_SetHPVolume(uint8_t left, uint8_t right);

/**
 * @brief 设置喇叭音量。
 * @param volume 喇叭音量，范围 0 到 63。
 * @return 0 表示成功；非 0 表示寄存器写入失败。
 */
uint8_t WM8978_SetSPKVolume(uint8_t volume);

/**
 * @brief 配置 WM8978 I2S 音频接口格式。
 * @param format 接口格式，当前工程使用 2 表示 I2S Philips。
 * @param length 数据位宽编码，当前工程使用 0 表示 16bit。
 * @return 0 表示成功；非 0 表示寄存器写入失败。
 */
uint8_t WM8978_ConfigI2S(uint8_t format, uint8_t length);

/**
 * @brief 打开 WM8978 播放输出通路。
 * @param 无。
 * @return 0 表示成功；非 0 表示寄存器写入失败。
 */
uint8_t WM8978_EnablePlayback(void);

/**
 * @brief 获取当前 WM8978 7bit IIC 地址。
 * @param 无。
 * @return 当前活动地址，正常为 0x1A。
 */
uint8_t WM8978_GetActiveAddress(void);

#ifdef __cplusplus
}
#endif

#endif
