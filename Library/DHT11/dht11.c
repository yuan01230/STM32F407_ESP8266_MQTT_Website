//
// Created by 标语 on 26-3-8.
//

/**
 * @file    dht11.c
 * @brief   DHT11 温湿度传感器驱动
 *
 * @note    DHT11 单总线通信协议时序详解：
 *
 * 1. 初始化/复位 (Reset)
 *    - 主机 (MCU) 设置数据线为输出模式。
 *    - 主机拉低数据线至少 18ms (DHT11_Rst 函数中实现了 20ms)。
 *    - 主机拉高数据线并保持 20-40us。
 *    - 主机切换为输入模式，等待 DHT11 响应。
 *
 * 2. 检测响应 (Check)
 *    - DHT11 检测到起始信号后，会发送一个响应信号：
 *      a) 拉低数据线约 80us。
 *      b) 拉高数据线约 80us。
 *    - 如果主机检测到这个低-高电平序列，说明 DHT11 存在且工作正常。
 *
 * 3. 数据传输
 *    - DHT11 发送 40 位 (5 字节) 数据，MSB 先行。
 *    - 数据格式：
 *      Byte 0: 湿度整数
 *      Byte 1: 湿度小数 (DHT11 通常为 0)
 *      Byte 2: 温度整数
 *      Byte 3: 温度小数 (DHT11 通常为 0)
 *      Byte 4: 校验和 (Byte0 + Byte1 + Byte2 + Byte3)
 *
 * 4. 数据位判定 ('0' vs '1')
 *    - 每一位数据都以 50us 的低电平开始 (时隙)。
 *    - 随后的高电平持续时间决定了数据位的值：
 *      a) '0': 高电平持续 26-28us。
 *      b) '1': 高电平持续 70us。
 *
 * 5. 结束
 *    - 40 位数据发送完毕后，DHT11 拉低总线 50us，然后释放总线。
 *    - 主机应恢复上拉状态 (输入模式，外部上拉或内部上拉)。
 */

#include "dht11.h"
#include "../delay/delay.h" // 包含新的延时库

// ============================================================================
// 内部辅助函数
// ============================================================================

/**
 * @brief 配置 GPIO 为推挽输出模式
 */
static void DHT11_Mode_OUT_PP(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/**
 * @brief 配置 GPIO 为输入模式 (无上下拉)
 */
static void DHT11_Mode_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 改为 PULLUP，增强抗干扰和稳定性，应对外部上拉不足的情况
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// 读写电平宏
#define DHT11_H     HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET)
#define DHT11_L     HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET)
#define DHT11_READ  HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)

/**
 * @brief 复位 DHT11 (主机发送起始信号)
 * @note  时序：主机拉低 >18ms -> 主机拉高 20-40us -> 切换输入
 */
static void DHT11_Rst(void)
{
    DHT11_Mode_OUT_PP(); // 切换为输出
    DHT11_L;             // 拉低总线
    delay_ms(20);        // 保持至少 18ms (使用精准毫秒延时)
    DHT11_H;             // 拉高总线
    delay_us(30);        // 等待 20-40us (使用精准微秒延时)
    DHT11_Mode_IN();     // 切换为输入，准备接收响应
}

/**
 * @brief 检测 DHT11 是否响应
 * @return 0: 存在 (响应正常), 1: 不存在
 * @note   时序：DHT11 拉低 80us -> DHT11 拉高 80us
 */
static uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;

    // 等待 DHT11 拉低 (响应信号的低电平部分)
    // 正常应该在 40-50us 内拉低
    while (DHT11_READ && retry < 200)
    {
        retry++;
        delay_us(1); // 增加超时到 200us
    }
    if (retry >= 200) return 1; // 超时未拉低

    retry = 0;
    // 等待 DHT11 拉高 (响应信号的高电平部分)
    // 低电平持续约 80us
    while (!DHT11_READ && retry < 200)
    {
        retry++;
        delay_us(1); 
    }
    if (retry >= 200) return 1; // 超时未拉高

    return 0; // 检测成功
}

/**
 * @brief 读取一个位 (Bit)
 * @return 0 或 1
 * @note   时序：50us 低电平 -> 26-28us 高电平 ('0') OR 70us 高电平 ('1')
 */
static uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0;

    // 等待上一位的低电平结束 (也是这一位的开始信号)
    // 如果已经在高电平，先等它走完（理论上不会，但增加鲁棒性）
    while (DHT11_READ && retry < 100) 
    {
        retry++;
        delay_us(1);
    }
    
    // 等待开始位的低电平 (50us) 结束，跳变到高电平
    retry = 0;
    while (!DHT11_READ && retry < 100) 
    {
        retry++;
        delay_us(1);
    }

    // 此时已经变成了高电平
    // 如果是 '0'，高电平持续 26-28us
    // 如果是 '1'，高电平持续 70us
    // 我们延时 40us 后检测，如果还是高电平，说明持续时间 > 40us，即为 '1'
    delay_us(40); // 延时 40us 后检测 (精准延时)

    if (DHT11_READ) return 1;
    else return 0;
}

/**
 * @brief 读取一个字节
 */
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, dat = 0;
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        dat |= DHT11_Read_Bit();
    }
    return dat;
}

// ============================================================================
// 外部接口函数
// ============================================================================

void DHT11_Init(void)
{
    // 初始化时先复位一下，确保总线空闲
    DHT11_Rst();
    // 最终保持输入状态，靠上拉电阻拉高
    DHT11_Mode_IN();
}

uint8_t DHT11_ReadData(DHT11_Data_t *data)
{
    uint8_t buf[5];
    uint8_t i;

    DHT11_Rst(); // 发送起始信号

    if (DHT11_Check() == 0) // 检测响应
    {
        // 读取 5 个字节
        // 湿度整数, 湿度小数, 温度整数, 温度小数, 校验和
        for (i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();
        }

        // 校验
        // 校验和 = 前四个字节之和
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            data->humi_int = buf[0];
            data->humi_dec = buf[1];
            data->temp_int = buf[2];
            data->temp_dec = buf[3];
            data->checksum = buf[4];
            return 0; // 成功
        }
    }

    return 1; // 失败
}
