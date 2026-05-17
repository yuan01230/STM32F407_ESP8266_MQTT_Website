#include "dht11.h"
#include <stdio.h>

/**
 * @brief DHT11 基础使用案例（单次执行）
 * @note
 * - 调用前需确保 `MX_GPIO_Init()` 和 `delay_init()` 已执行。
 * - 该示例仅演示单次读取，不负责 1 秒周期控制。
 */
void DHT11_Example_RunOnce(void)
{
    DHT11_Data_t sample;

    DHT11_Init();
    if (DHT11_ReadData(&sample) == 0U)
    {
        printf("DHT11 RH: %u.%u %%  TEMP: %u.%u C\r\n",
               sample.humi_int,
               sample.humi_dec,
               sample.temp_int,
               sample.temp_dec);
    }
    else
    {
        printf("DHT11 read failed\r\n");
    }
}
