光敏传感器ADC采集驱动使用说明
=====================================

【硬件连接】
- 光敏传感器模拟输出引脚 -> STM32 PF7 (ADC3_IN5)
- VCC -> 3.3V
- GND -> GND

【配置说明】
- ADC: ADC3
- 通道: Channel 5
- 分辨率: 12位 (0-4095)
- 参考电压: 3.3V
- 触发方式: 软件触发
- 转换模式: 单次转换

【使用方法】

1. 在main.c中添加头文件：
   #include "light_sensor.h"

2. 在main函数初始化部分调用（可选）：
   LightSensor_Init();

3. 使用示例：

// 示例1: 读取单次ADC值
uint16_t adc_value = LightSensor_ReadADC();
printf("ADC Value: %d\r\n", adc_value);

// 示例2: 读取平均ADC值（推荐）
uint16_t avg_adc = LightSensor_ReadADC_Average(10);
printf("Average ADC: %d\r\n", avg_adc);

// 示例3: 读取电压值
float voltage = LightSensor_GetVoltage();
printf("Voltage: %.2f V\r\n", voltage);

// 示例4: 获取光照等级
LightLevel_t level = LightSensor_GetLevel();
const char* level_str = LightSensor_GetLevelString();
printf("Light Level: %s\r\n", level_str);

// 示例5: 在LCD上显示
char lcd_buf[32];
uint16_t adc = LightSensor_ReadADC_Average(10);
float volt = LightSensor_GetVoltage();
sprintf(lcd_buf, "Light ADC:%4d", adc);
LCD_ShowString(10, 210, 240, 16, 16, lcd_buf);
sprintf(lcd_buf, "Voltage:%.2fV", volt);
LCD_ShowString(10, 230, 240, 16, 16, lcd_buf);
LCD_ShowString(10, 250, 240, 16, 16, LightSensor_GetLevelString());

【阈值调整】
如果光照等级判断不准确，可以修改light_sensor.c中LightSensor_GetLevel()函数的阈值：
- LIGHT_LEVEL_VERY_BRIGHT: adc_value < 500
- LIGHT_LEVEL_BRIGHT: adc_value < 1000
- LIGHT_LEVEL_NORMAL: adc_value < 2000
- LIGHT_LEVEL_DIM: adc_value < 3000
- LIGHT_LEVEL_DARK: adc_value >= 3000

根据实际传感器输出特性调整这些数值。

【注意事项】
1. 光敏电阻特性：一般光强越大，电阻越小，输出电压越低
2. 如果传感器输出特性相反，需要调整等级判断逻辑
3. 多次采样平均可以有效降低噪声干扰
4. ADC采样间隔建议不少于5ms
5. 确保在main.c中已调用MX_ADC3_Init()初始化ADC

【完整示例代码】（放在main.c的while(1)循环中）

// 定时读取光敏传感器
static uint32_t light_last_tick = 0;
uint32_t now = HAL_GetTick();
if (now - light_last_tick >= 500)  // 每500ms读取一次
{
    light_last_tick = now;
    
    // 读取ADC值
    uint16_t adc = LightSensor_ReadADC_Average(10);
    float voltage = LightSensor_GetVoltage();
    const char* level_str = LightSensor_GetLevelString();
    
    // 打印到串口
    printf("ADC:%d, Voltage:%.2fV, Level:%s\r\n", 
           adc, voltage, level_str);
    
    // 显示到LCD
    char buf[32];
    sprintf(buf, "ADC:%4d Volt:%.2fV", adc, voltage);
    LCD_ShowString(10, 210, 240, 16, 16, buf);
    LCD_ShowString(10, 230, 240, 16, 16, level_str);
}
