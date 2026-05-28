#include "light_sensor.h"

/* ========================= ????? ========================= */
/*
 * ??????????
 * 1. ?? ADC ????
 * 2. ??????????
 * 3. ? ADC ????????
 * 4. ?? ADC ????????
 *
 * ???????
 * - ADC3
 * - ?? 5
 * - CubeMX ????? ADC ???
 */

/* ========================= ????? ========================= */
#define LIGHT_SENSOR_ADC_MAX_VALUE          4095U
#define LIGHT_SENSOR_VREF                   3.3f
#define LIGHT_SENSOR_DEFAULT_AVG_TIMES      3U
#define LIGHT_SENSOR_INTER_SAMPLE_DELAY_MS  1U

/*
 * ???????
 * ADC ???????????????????
 * ????????????????????????
 */
#define LIGHT_SENSOR_ADC_VERY_BRIGHT_MAX    2600U
#define LIGHT_SENSOR_ADC_BRIGHT_MAX         2900U
#define LIGHT_SENSOR_ADC_NORMAL_MAX         3200U
#define LIGHT_SENSOR_ADC_DIM_MAX            3600U

/* ========================= ????? ========================= */
extern ADC_HandleTypeDef hadc3;

/* ========================= ????? ========================= */
void LightSensor_Init(void)
{
    /*
     * ?? ADC ?? MX_ADC3_Init() ??????
     * ????????????????????????????
     */
}

uint16_t LightSensor_ReadADC(void)
{
    uint16_t adc_value = 0U;

    HAL_ADC_Start(&hadc3);
    if (HAL_ADC_PollForConversion(&hadc3, 100U) == HAL_OK)
    {
        adc_value = (uint16_t)HAL_ADC_GetValue(&hadc3);
    }
    HAL_ADC_Stop(&hadc3);

    return adc_value;
}

uint16_t LightSensor_ReadADCAverage(uint8_t times)
{
    uint32_t sum = 0U;
    uint8_t sample_times = (times == 0U) ? 1U : times;

    for (uint8_t i = 0U; i < sample_times; ++i)
    {
        sum += LightSensor_ReadADC();
        HAL_Delay(LIGHT_SENSOR_INTER_SAMPLE_DELAY_MS);
    }

    return (uint16_t)(sum / sample_times);
}

float LightSensor_ConvertToVoltage(uint16_t adc_value)
{
    return ((float)adc_value / (float)LIGHT_SENSOR_ADC_MAX_VALUE) * LIGHT_SENSOR_VREF;
}

/* ========================= ????? ========================= */
LightLevel_t LightSensor_GetLevelByADC(uint16_t adc_value)
{
    if (adc_value < LIGHT_SENSOR_ADC_VERY_BRIGHT_MAX)
    {
        return LIGHT_LEVEL_VERY_BRIGHT;
    }
    if (adc_value < LIGHT_SENSOR_ADC_BRIGHT_MAX)
    {
        return LIGHT_LEVEL_BRIGHT;
    }
    if (adc_value < LIGHT_SENSOR_ADC_NORMAL_MAX)
    {
        return LIGHT_LEVEL_NORMAL;
    }
    if (adc_value < LIGHT_SENSOR_ADC_DIM_MAX)
    {
        return LIGHT_LEVEL_DIM;
    }

    return LIGHT_LEVEL_DARK;
}

const char *LightSensor_LevelToString(LightLevel_t level)
{
    switch (level)
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

/* ========================= ????? ========================= */
uint16_t LightSensor_ReadADC_Average(uint8_t times)
{
    return LightSensor_ReadADCAverage(times);
}

float LightSensor_GetVoltage(void)
{
    return LightSensor_ConvertToVoltage(LightSensor_ReadADCAverage(LIGHT_SENSOR_DEFAULT_AVG_TIMES));
}

LightLevel_t LightSensor_GetLevel(void)
{
    return LightSensor_GetLevelByADC(LightSensor_ReadADCAverage(LIGHT_SENSOR_DEFAULT_AVG_TIMES));
}

const char *LightSensor_GetLevelString(void)
{
    return LightSensor_LevelToString(LightSensor_GetLevel());
}
