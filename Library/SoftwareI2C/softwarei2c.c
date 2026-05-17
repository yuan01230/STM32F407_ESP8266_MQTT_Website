//
// Created by 标语 on 26-3-7.
//

#include "softwarei2c.h"
#include "../delay/delay.h" // 包含延时库

// ============================================================================
// 内部变量与辅助函数
// ============================================================================

// 默认延时时间 (微秒)，可通过 SoftwareI2C_SetDelay 修改
// 5us 对应约 100kHz 半周期
static volatile uint32_t i2c_delay_us = 5;

/**
 * @brief 设置 I2C 半周期延时 (微秒)
 * @param us 延时微秒数
 *           - 5: ~100kHz (标准模式)
 *           - 2: ~250kHz
 *           - 1: ~400kHz (快速模式，加上GPIO开销)
 */
void SoftwareI2C_SetDelay(uint32_t us)
{
    if (us == 0) us = 1;
    i2c_delay_us = us;
}

/**
 * @brief I2C 延时函数
 */
static void I2C_Delay(void)
{
    delay_us(i2c_delay_us);
}

// 写 SCL 电平
static void I2C_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);
}

static void I2C_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);
}

#define I2C_W_SCL(x)    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
// 写 SDA 电平
#define I2C_W_SDA(x)    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
// 读 SDA 电平
#define I2C_R_SDA()     HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN)

// ============================================================================
// 驱动实现
// ============================================================================

/**
 * @brief 初始化软件 I2C 引脚
 * @note  配置 SCL 和 SDA 为推挽输出，上拉。读取 ACK/数据前会临时切换 SDA 为输入。
 */
void SoftwareI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 开启 GPIO 时钟 (假设 SCL 和 SDA 在同一个端口，如果不同请分别开启)
    if (I2C_SCL_PORT == GPIOB || I2C_SDA_PORT == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    if (I2C_SCL_PORT == GPIOA || I2C_SDA_PORT == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    if (I2C_SCL_PORT == GPIOC || I2C_SDA_PORT == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    // ... 其他端口请自行添加

    // 通用配置：推挽输出，上拉，高速；与官方示例保持一致。
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;         // 上拉 (如果外部有上拉电阻，这里也可以设为 NOPULL)
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    // 配置 SCL
    GPIO_InitStruct.Pin = I2C_SCL_PIN;
    HAL_GPIO_Init(I2C_SCL_PORT, &GPIO_InitStruct);

    // 配置 SDA
    GPIO_InitStruct.Pin = I2C_SDA_PIN;
    HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);

    // 初始状态：释放总线 (高电平)
    I2C_W_SCL(1);
    I2C_W_SDA(1);
}

/**
 * @brief 产生 I2C 起始信号
 * @note  SCL 高电平期间，SDA 从高变低
 */
void SoftwareI2C_Start(void)
{
    I2C_SDA_OUT();
    I2C_W_SDA(1);
    I2C_W_SCL(1);
    I2C_Delay();
    I2C_W_SDA(0);
    I2C_Delay();
    I2C_W_SCL(0); // 钳住总线，准备发送数据
}

/**
 * @brief 产生 I2C 停止信号
 * @note  SCL 高电平期间，SDA 从低变高
 */
void SoftwareI2C_Stop(void)
{
    I2C_SDA_OUT();
    I2C_W_SDA(0);
    I2C_W_SCL(1);
    I2C_Delay();
    I2C_W_SDA(1);
    I2C_Delay();
}

/**
 * @brief 发送一个字节
 * @param byte 要发送的数据
 */
void SoftwareI2C_SendByte(uint8_t byte)
{
    I2C_SDA_OUT();
    for (uint8_t i = 0; i < 8; i++)
    {
        // 先准备数据
        I2C_W_SDA(byte & (0x80 >> i));
        I2C_Delay();
        // 拉高 SCL，从机读取数据
        I2C_W_SCL(1);
        I2C_Delay();
        // 拉低 SCL，准备下一位
        I2C_W_SCL(0);
        I2C_Delay();
    }
}

/**
 * @brief 接收一个字节
 * @return 接收到的数据
 */
uint8_t SoftwareI2C_ReceiveByte(void)
{
    uint8_t byte = 0;

    I2C_SDA_IN();

    // 主机释放 SDA 线，准备读取
    I2C_W_SDA(1);
    I2C_Delay(); // 增加延时，确保 SDA 释放完成

    for (uint8_t i = 0; i < 8; i++)
    {
        // 拉高 SCL，主机读取数据
        I2C_W_SCL(1);
        I2C_Delay();

        if (I2C_R_SDA())
        {
            byte |= (0x80 >> i);
        }

        // 拉低 SCL
        I2C_W_SCL(0);
        I2C_Delay();
    }
    return byte;
}

/**
 * @brief 发送 ACK 或 NACK
 * @param ack 0: ACK (应答), 1: NACK (非应答)
 */
void SoftwareI2C_SendACK(uint8_t ack)
{
    I2C_SDA_OUT();
    I2C_W_SDA(ack);
    I2C_Delay();
    I2C_W_SCL(1);
    I2C_Delay();
    I2C_W_SCL(0);
    I2C_Delay();
}

/**
 * @brief 等待从机 ACK
 * @return 0: ACK 接收成功, 1: NACK (接收失败)
 */
static uint8_t SoftwareI2C_WaitACKInternal(uint8_t stop_on_nack)
{
    uint8_t ack;
    uint16_t timeout = 0U;

    // 主机释放 SDA，等待从机拉低
    I2C_SDA_IN();
    I2C_W_SCL(0);
    I2C_SDA_OUT();
    I2C_W_SDA(1);
    I2C_Delay();
    I2C_SDA_IN();
    I2C_Delay();
    I2C_W_SCL(1);
    I2C_Delay();

    while (I2C_R_SDA())
    {
        timeout++;
        if (timeout > 1000U)
        {
            ack = 1U;
            if (stop_on_nack != 0U)
            {
                SoftwareI2C_Stop();
            }
            I2C_W_SCL(0);
            I2C_Delay();
            return ack;
        }
        I2C_Delay();
    }

    ack = 0U;
    I2C_W_SCL(0);
    I2C_Delay();

    return ack;
}

uint8_t SoftwareI2C_WaitACK(void)
{
    return SoftwareI2C_WaitACKInternal(1U);
}

uint8_t SoftwareI2C_WaitACKNoStop(void)
{
    return SoftwareI2C_WaitACKInternal(0U);
}
