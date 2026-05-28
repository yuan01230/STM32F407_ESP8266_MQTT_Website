#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 WM8978 + I2S2 DMA 方波测试音。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 0 表示成功；非 0 表示参数、WM8978 或 I2S DMA 启动失败。
 */
uint8_t AudioTest_StartTone(I2S_HandleTypeDef *hi2s);

/**
 * @brief 停止方波测试音。
 * @param hi2s I2S 句柄，允许为 NULL。
 * @return 无。
 */
void AudioTest_StopTone(I2S_HandleTypeDef *hi2s);

#ifdef __cplusplus
}
#endif

#endif
