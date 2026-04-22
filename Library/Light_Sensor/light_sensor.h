//
// Created by 标语 on 26-1-21.
//

#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "main.h"
#include "stm32f4xx_hal.h"

/* ADC相关宏定义 */
#define LIGHT_SENSOR_ADC            ADC3
#define LIGHT_SENSOR_ADC_CHANNEL    ADC_CHANNEL_5
#define LIGHT_SENSOR_GPIO_PORT      GPIOF
#define LIGHT_SENSOR_GPIO_PIN       GPIO_PIN_7

/* ADC采样参数 */
#define ADC_MAX_VALUE               4095        // 12位ADC最大值
#define ADC_VREF                    3.3f        // 参考电压(V)
#define ADC_AVERAGE_TIMES           10          // 多次采样取平均

/* 光照强度等级定义 */
typedef enum {
    LIGHT_LEVEL_DARK = 0,       // 黑暗
    LIGHT_LEVEL_DIM,            // 昏暗
    LIGHT_LEVEL_NORMAL,         // 正常
    LIGHT_LEVEL_BRIGHT,         // 明亮
    LIGHT_LEVEL_VERY_BRIGHT     // 非常明亮
} LightLevel_t;

/* 函数声明 */
void LightSensor_Init(void);
uint16_t LightSensor_ReadADC(void);
uint16_t LightSensor_ReadADC_Average(uint8_t times);
float LightSensor_GetVoltage(void);
LightLevel_t LightSensor_GetLevel(void);
const char* LightSensor_GetLevelString(void);

#endif //LIGHT_SENSOR_H
