#ifndef BEEP_H
#define BEEP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief 蜂鸣器逻辑状态定义
 */
typedef enum {
    BEEP_OFF = 0, /**< 关闭蜂鸣器 */
    BEEP_ON = 1   /**< 打开蜂鸣器 */
} BEEP_Status_t;

/**
 * @brief 初始化蜂鸣器驱动
 * @note
 * - 该函数不初始化 GPIO 外设
 * - 当前策略为上电后默认关闭蜂鸣器
 */
void BEEP_Init(void);

/**
 * @brief 打开蜂鸣器
 */
void BEEP_On(void);

/**
 * @brief 关闭蜂鸣器
 */
void BEEP_Off(void);

/**
 * @brief 翻转蜂鸣器当前状态
 */
void BEEP_Toggle(void);

/**
 * @brief 设置蜂鸣器状态
 * @param status 目标状态：BEEP_ON 或 BEEP_OFF
 */
void BEEP_Set(BEEP_Status_t status);

#ifdef __cplusplus
}
#endif

#endif
