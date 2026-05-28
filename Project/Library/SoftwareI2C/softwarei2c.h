//
// Created by 标语 on 26-3-7.
//

#ifndef SOFTWAREI2C_H
#define SOFTWAREI2C_H

#include "main.h"

// ============================================================================
// 引脚定义 (请根据实际硬件修改)
// ============================================================================
// 原理图 IIC 总线: PB8 -> IIC_SCL, PB9 -> IIC_SDA。
#ifndef I2C_SCL_PORT
#define I2C_SCL_PORT    GPIOB
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN     GPIO_PIN_8
#endif

#ifndef I2C_SDA_PORT
#define I2C_SDA_PORT    GPIOB
#endif
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN     GPIO_PIN_9
#endif

// ============================================================================
// 速率延时参数定义 (基于 STM32F4 @ 168MHz)
// ============================================================================
// 理论计算：
// 100kHz -> 半周期 5us. 循环耗时约 35ns. 理论 count = 140.
// 400kHz -> 半周期 1.25us. 理论 count = 35.
//
// 为了保证通信稳定性，这里采用保守值（不扣除 GPIO 开销，甚至略微放大）。
// 实际频率会略低于标称值（例如 100k 实际可能为 80-90k），但这能显著提高信号质量。

#define I2C_DELAY_100K    150  // 标准模式 (实际约 80-90kHz)
#define I2C_DELAY_400K    40   // 快速模式 (实际约 350kHz)
#define I2C_DELAY_10K     800  // 低速模式 (用于长距离或高电容负载)


// ============================================================================
// 函数声明
// ============================================================================

/**
 * @brief 初始化软件 I2C 引脚 (配置为开漏输出)
 */
void SoftwareI2C_Init(void);

/**
 * @brief 设置 I2C 延时计数，用于控制通信速率
 * @param count 延时循环次数。推荐使用宏定义:
 *              - I2C_DELAY_100K
 *              - I2C_DELAY_400K
 */
void SoftwareI2C_SetDelay(uint32_t count);

/**
 * @brief 产生 I2C 起始信号
 */
void SoftwareI2C_Start(void);

/**
 * @brief 产生 I2C 停止信号
 */
void SoftwareI2C_Stop(void);

/**
 * @brief 发送一个字节
 * @param byte 要发送的数据
 */
void SoftwareI2C_SendByte(uint8_t byte);

/**
 * @brief 接收一个字节
 * @return 接收到的数据
 */
uint8_t SoftwareI2C_ReceiveByte(void);

/**
 * @brief 发送 ACK 或 NACK
 * @param ack 0: ACK, 1: NACK
 */
void SoftwareI2C_SendACK(uint8_t ack);

/**
 * @brief 等待从机 ACK
 * @return 0: ACK 接收成功, 1: NACK (接收失败)
 */
uint8_t SoftwareI2C_WaitACK(void);

uint8_t SoftwareI2C_WaitACKNoStop(void);

#endif //SOFTWAREI2C_H
