//
// Created by 标语 on 26-3-6.
//

#include "beep.h"

/**
  * @brief  打开蜂鸣器
  * @note   设置 BEEP 引脚为高电平
  */
void BEEP_On(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
}

/**
  * @brief  关闭蜂鸣器
  * @note   设置 BEEP 引脚为低电平
  */
void BEEP_Off(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  翻转蜂鸣器状态
  * @note   翻转 BEEP 引脚电平
  */
void BEEP_Toggle(void)
{
    HAL_GPIO_TogglePin(BEEP_GPIO_Port, BEEP_Pin);
}

/**
  * @brief  设置蜂鸣器状态
  * @param  status: BEEP_ON 或 BEEP_OFF
  */
void BEEP_Set(BEEP_Status_t status)
{
    if (status == BEEP_ON)
    {
        BEEP_On();
    }
    else
    {
        BEEP_Off();
    }
}
