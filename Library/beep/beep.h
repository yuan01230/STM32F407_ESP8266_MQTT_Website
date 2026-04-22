//
// Created by 标语 on 26-3-6.
//

#ifndef BEEP_H
#define BEEP_H

#include "main.h"

/**
 * @brief BEEP 状态枚举
 */
typedef enum {
    BEEP_OFF = 0, // 关闭
    BEEP_ON = 1   // 打开
} BEEP_Status_t;


/* 函数声明 */

/**
  * @brief  打开蜂鸣器
  * @note   假设高电平触发蜂鸣器
  */
void BEEP_On(void);

/**
  * @brief  关闭蜂鸣器
  * @note   假设低电平关闭蜂鸣器
  */
void BEEP_Off(void);

/**
  * @brief  翻转蜂鸣器状态
  */
void BEEP_Toggle(void);

/**
  * @brief  设置蜂鸣器状态
  * @param  status: BEEP_ON 或 BEEP_OFF
  */
void BEEP_Set(BEEP_Status_t status);


#endif //BEEP_H
