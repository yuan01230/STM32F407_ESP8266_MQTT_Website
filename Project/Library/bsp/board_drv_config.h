#ifndef BOARD_DRV_CONFIG_H
#define BOARD_DRV_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "gpio_device.h"

/**
 * @brief LED 板级配置
 * @note
 * - `BOARD_LED_COUNT`：LED 实际数量
 * - `BOARD_LED_CONFIG_TABLE`：LED 静态注册表
 * - 当前板卡（普中-麒麟 F407）：LED0/LED1 为低电平点亮
 */
#define BOARD_LED_COUNT 2U
#define BOARD_LED_CONFIG_TABLE \
    { LED0_GPIO_Port, LED0_Pin, GPIO_ACTIVE_LOW }, \
    { LED1_GPIO_Port, LED1_Pin, GPIO_ACTIVE_LOW }

/**
 * @brief 蜂鸣器板级配置
 * @note 当前板卡 BEEP 引脚为 PF8，逻辑高有效
 */
#define BOARD_BEEP_CONFIG { BEEP_GPIO_Port, BEEP_Pin, GPIO_ACTIVE_HIGH }

/**
 * @brief 按键板级配置
 * @note
 * - `BOARD_KEY_COUNT`：按键实际数量
 * - `BOARD_KEY_CONFIG_TABLE`：按键静态注册表
 * - 当前板卡：KEY0/KEY1/KEY2 为低电平按下，KEY_UP 为高电平按下
 */
#define BOARD_KEY_COUNT 4U
#define BOARD_KEY_CONFIG_TABLE \
    { KEY0_GPIO_Port, KEY0_Pin, GPIO_ACTIVE_LOW }, \
    { KEY1_GPIO_Port, KEY1_Pin, GPIO_ACTIVE_LOW }, \
    { KEY2_GPIO_Port, KEY2_Pin, GPIO_ACTIVE_LOW }, \
    { KEY_UP_GPIO_Port, KEY_UP_Pin, GPIO_ACTIVE_HIGH }

#ifdef __cplusplus
}
#endif

#endif
