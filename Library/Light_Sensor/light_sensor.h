#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief 光照等级枚举
 * @note
 * - 等级划分基于当前模块中的 ADC 阈值表。
 * - 若不同传感器分压特性不同，只需调整 `light_sensor.c` 中阈值。
 */
typedef enum
{
    LIGHT_LEVEL_DARK = 0,
    LIGHT_LEVEL_DIM,
    LIGHT_LEVEL_NORMAL,
    LIGHT_LEVEL_BRIGHT,
    LIGHT_LEVEL_VERY_BRIGHT
} LightLevel_t;

/**
 * @brief 初始化光敏传感器驱动
 * @note
 * - 当前实现不重复初始化 ADC，仅保留统一入口。
 * - 使用前需确保 `MX_ADC3_Init()` 已执行。
 */
void LightSensor_Init(void);

/**
 * @brief 读取一次原始 ADC 值
 * @return uint16_t 原始 ADC 值，范围 0~4095
 */
uint16_t LightSensor_ReadADC(void);

/**
 * @brief 多次采样并求平均值
 * @param times 采样次数
 * @return uint16_t 平均 ADC 值
 * @note
 * - 当 `times==0` 时会自动按 1 次处理。
 */
uint16_t LightSensor_ReadADCAverage(uint8_t times);

/**
 * @brief 将 ADC 值换算为电压
 * @param adc_value 原始 ADC 值
 * @return float 电压值（单位：V）
 */
float LightSensor_ConvertToVoltage(uint16_t adc_value);

/**
 * @brief 根据 ADC 值判断光照等级
 * @param adc_value 原始 ADC 值
 * @return LightLevel_t 光照等级
 */
LightLevel_t LightSensor_GetLevelByADC(uint16_t adc_value);

/**
 * @brief 获取光照等级对应的字符串描述
 * @param level 光照等级
 * @return const char* 等级字符串
 */
const char *LightSensor_LevelToString(LightLevel_t level);

/**
 * @brief 兼容旧接口：读取平均 ADC 值
 * @param times 采样次数
 * @return uint16_t 平均 ADC 值
 */
uint16_t LightSensor_ReadADC_Average(uint8_t times);

/**
 * @brief 兼容旧接口：直接读取默认平均次数下的电压
 * @return float 电压值（单位：V）
 */
float LightSensor_GetVoltage(void);

/**
 * @brief 兼容旧接口：直接读取默认平均次数下的光照等级
 * @return LightLevel_t 光照等级
 */
LightLevel_t LightSensor_GetLevel(void);

/**
 * @brief 兼容旧接口：直接返回默认平均次数下的等级字符串
 * @return const char* 等级字符串
 */
const char *LightSensor_GetLevelString(void);

#ifdef __cplusplus
}
#endif

#endif
