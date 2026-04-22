//
// Created by 标语 on 25-12-30.
//

/**
 ******************************************************************************
 * @file    led.c
 * @brief   LED driver based on STM32 HAL GPIO
 ******************************************************************************
 * @attention
 *
 * 本文件封装了 LED 的常用操作：
 *  - 单个 LED 开 / 关
 *  - 全部 LED 开 / 关
 *  - 翻转
 *  - 阻塞式闪烁
 *
 * 依赖：
 *  - STM32 HAL
 *  - GPIO 已由 CubeMX 初始化
 *
 ******************************************************************************
 */

#include "led.h"

/* ===================== LED 硬件映射表 ===================== */

/**
 * @brief LED GPIO 端口映射
 */
static GPIO_TypeDef *LED_GPIO_Port[LED_NUM] =
{
    LED0_GPIO_Port,
    LED1_GPIO_Port
};

/**
 * @brief LED GPIO 引脚映射
 */
static uint16_t LED_GPIO_Pin[LED_NUM] =
{
    LED0_Pin,
    LED1_Pin
};

/* ===================== 接口函数实现 ===================== */

void LED_Init(void)
{
    /* 当前 GPIO 初始化已在 MX_GPIO_Init 中完成
       此函数预留用于后续扩展 */
    LED_AllOff();
}

void LED_Off(LED_TypeDef led)
{
    if (led >= LED_NUM) return;

    HAL_GPIO_WritePin(LED_GPIO_Port[led],
                      LED_GPIO_Pin[led],
                      GPIO_PIN_SET);
}

void LED_On(LED_TypeDef led)
{
    if (led >= LED_NUM) return;

    HAL_GPIO_WritePin(LED_GPIO_Port[led],
                      LED_GPIO_Pin[led],
                      GPIO_PIN_RESET);
}

void LED_Toggle(LED_TypeDef led)
{
    if (led >= LED_NUM) return;

    HAL_GPIO_TogglePin(LED_GPIO_Port[led],
                       LED_GPIO_Pin[led]);
}

void LED_AllOn(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        LED_On((LED_TypeDef)i);
    }
}

void LED_AllOff(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        LED_Off((LED_TypeDef)i);
    }
}

void LED_Blink(LED_TypeDef led, uint32_t period_ms, uint8_t times)
{
    if (led >= LED_NUM) return;

    for (uint8_t i = 0; i < times; i++)
    {
        LED_On(led);
        HAL_Delay(period_ms / 2);

        LED_Off(led);
        HAL_Delay(period_ms / 2);
    }
}

