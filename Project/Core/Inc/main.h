/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/**
  * @brief 应用层传感器快照。
  * @note
  * - 这些值由主循环中的 LCD_Test_Run() 周期刷新。
  * - MQTT 模块只读取这份缓存，不在网络回调里直接阻塞读取 DHT11/ADC。
  * - 这样可以让 MX_LWIP_Process() 保持轻量，避免网络收发被传感器时序拖慢。
  */
typedef struct
{
  float temperature;    /**< DHT11 温度，单位：摄氏度。 */
  float humidity;       /**< DHT11 湿度，单位：%RH。 */
  uint16_t light_adc;   /**< 光敏传感器 ADC 原始值，范围约 0~4095。 */
  float light_voltage;  /**< 光敏传感器换算电压，单位：V。 */
  float pitch;          /**< MPU6050 DMP 解算的俯仰角。 */
  float roll;           /**< MPU6050 DMP 解算的横滚角。 */
  float yaw;            /**< MPU6050 DMP 解算的航向角。 */
} App_SensorSnapshot_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/**
  * @brief 获取最近一次主循环采集到的传感器快照。
  * @param snapshot 输出参数，传入有效指针后函数会填充最新缓存值。
  */
void App_GetSensorSnapshot(App_SensorSnapshot_t *snapshot);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY2_Pin GPIO_PIN_2
#define KEY2_GPIO_Port GPIOE
#define KEY1_Pin GPIO_PIN_3
#define KEY1_GPIO_Port GPIOE
#define KEY0_Pin GPIO_PIN_4
#define KEY0_GPIO_Port GPIOE
#define ESP8266EN_Pin GPIO_PIN_6
#define ESP8266EN_GPIO_Port GPIOF
#define BEEP_Pin GPIO_PIN_8
#define BEEP_GPIO_Port GPIOF
#define LED0_Pin GPIO_PIN_9
#define LED0_GPIO_Port GPIOF
#define LED1_Pin GPIO_PIN_10
#define LED1_GPIO_Port GPIOF
#define ESP8266EST_Pin GPIO_PIN_0
#define ESP8266EST_GPIO_Port GPIOC
#define KEY_UP_Pin GPIO_PIN_0
#define KEY_UP_GPIO_Port GPIOA
#define SPI1_CS_Pin GPIO_PIN_14
#define SPI1_CS_GPIO_Port GPIOB
#define LCD_BL_Pin GPIO_PIN_15
#define LCD_BL_GPIO_Port GPIOB
#define DHT11_Pin GPIO_PIN_9
#define DHT11_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */
#define ETH_RESET_Pin GPIO_PIN_3
#define ETH_RESET_GPIO_Port GPIOD

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
