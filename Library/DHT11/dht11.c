#include "dht11.h"
#include "../delay/delay.h"

/*
 * DHT11 驱动设计说明：
 * 1) 本模块只负责单总线时序驱动和 5 字节数据解析。
 * 2) 上层无需感知 GPIO 输入/输出切换细节，只调用 Init/ReadData。
 * 3) 若后续更换 DHT22 等兼容器件，可在本文件内部替换协议细节。
 */

#define DHT11_PORT GPIOG
#define DHT11_PIN  GPIO_PIN_9

#define DHT11_HIGH() HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET)
#define DHT11_LOW()  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET)
#define DHT11_READ() HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)

/**
 * @brief 将 DHT11 数据引脚配置为推挽输出
 */
static void dht11_mode_output(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT11_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &gpio_init);
}

/**
 * @brief 将 DHT11 数据引脚配置为输入上拉
 */
static void dht11_mode_input(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT11_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &gpio_init);
}

/**
 * @brief 主机向 DHT11 发送起始信号
 */
static void dht11_reset(void)
{
    dht11_mode_output();
    DHT11_LOW();
    delay_ms(20);
    DHT11_HIGH();
    delay_us(30);
    dht11_mode_input();
}

/**
 * @brief 检查 DHT11 是否给出响应
 * @return uint8_t 0=响应正常，1=无响应或超时
 */
static uint8_t dht11_check(void)
{
    uint8_t retry = 0U;

    while (DHT11_READ() != GPIO_PIN_RESET && retry < 200U)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 200U)
    {
        return 1U;
    }

    retry = 0U;
    while (DHT11_READ() == GPIO_PIN_RESET && retry < 200U)
    {
        retry++;
        delay_us(1);
    }

    return (retry >= 200U) ? 1U : 0U;
}

/**
 * @brief 读取 DHT11 传输的 1 bit 数据
 * @return uint8_t 0 或 1
 */
static uint8_t dht11_read_bit(void)
{
    uint8_t retry = 0U;

    while (DHT11_READ() != GPIO_PIN_RESET && retry < 100U)
    {
        retry++;
        delay_us(1);
    }

    retry = 0U;
    while (DHT11_READ() == GPIO_PIN_RESET && retry < 100U)
    {
        retry++;
        delay_us(1);
    }

    delay_us(40);
    return (DHT11_READ() != GPIO_PIN_RESET) ? 1U : 0U;
}

/**
 * @brief 读取 DHT11 传输的 1 字节数据
 * @return uint8_t 读取结果
 */
static uint8_t dht11_read_byte(void)
{
    uint8_t i;
    uint8_t data = 0U;

    for (i = 0U; i < 8U; i++)
    {
        data <<= 1U;
        data |= dht11_read_bit();
    }

    return data;
}

void DHT11_Init(void)
{
    dht11_reset();
    dht11_mode_input();
}

uint8_t DHT11_ReadData(DHT11_Data_t *data)
{
    uint8_t i;
    uint8_t raw[5];

    if (data == NULL)
    {
        return 1U;
    }

    dht11_reset();
    if (dht11_check() != 0U)
    {
        return 1U;
    }

    for (i = 0U; i < 5U; i++)
    {
        raw[i] = dht11_read_byte();
    }

    if ((uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]) != raw[4])
    {
        return 1U;
    }

    data->humi_int = raw[0];
    data->humi_dec = raw[1];
    data->temp_int = raw[2];
    data->temp_dec = raw[3];
    data->checksum = raw[4];
    return 0U;
}
