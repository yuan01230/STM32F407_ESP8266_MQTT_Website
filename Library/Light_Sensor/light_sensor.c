//
// Created by 标语 on 26-1-21.
//

#include "light_sensor.h"
#include "stdio.h"

/* 外部变量引用 */
extern ADC_HandleTypeDef hadc3;

/**
 * @brief  初始化光敏传感器
 * @note   如果已在STM32CubeMX中配置好，此函数可以为空
 *         主要用于校准ADC（可选）
 * @param  无
 * @retval 无
 */
void LightSensor_Init(void)
{
    // ADC已在main.c中通过MX_ADC3_Init()初始化
    // 此处可以添加ADC校准代码（可选）
    // HAL_ADCEx_Calibration_Start(&hadc3);
}

/**
 * @brief  读取一次ADC值
 * @note   轮询方式读取ADC转换结果
 * @param  无
 * @retval ADC原始值 (0-4095)
 */
uint16_t LightSensor_ReadADC(void)
{
    uint16_t adc_value = 0;
    
    // 启动ADC转换
    HAL_ADC_Start(&hadc3);
    
    // 等待转换完成，超时时间100ms
    if(HAL_ADC_PollForConversion(&hadc3, 100) == HAL_OK)
    {
        // 读取ADC转换结果
        adc_value = HAL_ADC_GetValue(&hadc3);
    }
    
    // 停止ADC转换
    HAL_ADC_Stop(&hadc3);
    
    return adc_value;
}

/**
 * @brief  多次采样ADC并求平均值
 * @note   通过多次采样取平均，降低噪声干扰
 * @param  times: 采样次数 (1-255)
 * @retval 平均ADC值 (0-4095)
 */
uint16_t LightSensor_ReadADC_Average(uint8_t times)
{
    uint32_t sum = 0;
    uint16_t avg_value = 0;
    
    // 参数有效性检查
    if(times == 0) times = 1;
    
    // 多次采样
    for(uint8_t i = 0; i < times; i++)
    {
        sum += LightSensor_ReadADC();
        HAL_Delay(5);  // 等待5ms，避免采样过快
    }
    
    // 计算平均值
    avg_value = sum / times;
    
    return avg_value;
}

/**
 * @brief  获取光敏传感器电压值
 * @note   将ADC值转换为实际电压(V)
 * @param  无
 * @retval 电压值 (0.0 - 3.3V)
 */
float LightSensor_GetVoltage(void)
{
    uint16_t adc_value = 0;
    float voltage = 0.0f;
    
    // 读取平均ADC值
    adc_value = LightSensor_ReadADC_Average(ADC_AVERAGE_TIMES);
    
    // 转换为电压值: Voltage = (ADC_Value / ADC_Max) * Vref
    voltage = ((float)adc_value / ADC_MAX_VALUE) * ADC_VREF;
    
    return voltage;
}

/**
 * @brief  获取光照强度等级
 * @note   根据ADC值划分光照强度等级
 *         阈值可根据实际传感器特性调整
 * @param  无
 * @retval 光照强度等级
 */
LightLevel_t LightSensor_GetLevel(void)
{
    uint16_t adc_value = 0;
    LightLevel_t level = LIGHT_LEVEL_NORMAL;
    
    // 读取平均ADC值
    adc_value = LightSensor_ReadADC_Average(ADC_AVERAGE_TIMES);
    
    // 根据ADC值划分等级（注意：光敏电阻一般光强越大，电压越低）
    // 阈值可根据实际情况调整
    if(adc_value < 500)
    {
        level = LIGHT_LEVEL_VERY_BRIGHT;  // 非常明亮 (电压低)
    }
    else if(adc_value < 1000)
    {
        level = LIGHT_LEVEL_BRIGHT;        // 明亮
    }
    else if(adc_value < 2000)
    {
        level = LIGHT_LEVEL_NORMAL;        // 正常
    }
    else if(adc_value < 3000)
    {
        level = LIGHT_LEVEL_DIM;           // 昏暗
    }
    else
    {
        level = LIGHT_LEVEL_DARK;          // 黑暗 (电压高)
    }
    
    return level;
}

/**
 * @brief  获取光照强度等级字符串
 * @note   返回当前光照强度的描述字符串
 * @param  无
 * @retval 光照强度描述字符串
 */
const char* LightSensor_GetLevelString(void)
{
    LightLevel_t level = LightSensor_GetLevel();
    
    switch(level)
    {
        case LIGHT_LEVEL_DARK:
            return "Dark";
        case LIGHT_LEVEL_DIM:
            return "Dim";
        case LIGHT_LEVEL_NORMAL:
            return "Normal";
        case LIGHT_LEVEL_BRIGHT:
            return "Bright";
        case LIGHT_LEVEL_VERY_BRIGHT:
            return "Very Bright";
        default:
            return "Unknown";
    }
}
