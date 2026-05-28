#include "light_sensor.h"
#include <stdio.h>

/**
 * @brief 光敏传感器基础使用案例（单次执行）
 * @note
 * - 调用前需确保 `MX_ADC3_Init()` 已执行。
 * - 示例中先采样，再基于同一份 ADC 值计算电压和等级，避免重复读取。
 */
void LightSensor_Example_RunOnce(void)
{
    uint16_t adc_value;
    float voltage;
    LightLevel_t level;

    LightSensor_Init();

    adc_value = LightSensor_ReadADCAverage(10U);
    voltage = LightSensor_ConvertToVoltage(adc_value);
    level = LightSensor_GetLevelByADC(adc_value);

    printf("Light ADC: %u  Voltage: %.2f V  Level: %s\r\n",
           adc_value,
           voltage,
           LightSensor_LevelToString(level));
}
