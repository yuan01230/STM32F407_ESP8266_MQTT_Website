//
// Created by 标语 on 26-3-7.
//

#ifndef AT24CXX_H
#define AT24CXX_H

#include "main.h"

// ============================================================================
// AT24C 系列芯片型号定义
// ============================================================================
#define AT24C01     127
#define AT24C02     255
#define AT24C04     511
#define AT24C08     1023
#define AT24C16     2047
#define AT24C32     4095
#define AT24C64     8191
#define AT24C128    16383
#define AT24C256    32767

// 请在此处选择您使用的芯片型号
#define EE_TYPE     AT24C02

// 设备地址 (A0/A1/A2 接地时为 0xA0)
#define AT24CXX_ADDR 0xA0

// ============================================================================
// 函数声明
// ============================================================================

/**
 * @brief 初始化 AT24CXX (实际上是初始化 I2C)
 */
void AT24CXX_Init(void);

/**
 * @brief 在 AT24CXX 指定地址读取一个字节
 * @param ReadAddr: 开始读取的地址 (0 ~ EE_TYPE)
 * @return 读取到的数据
 */
uint8_t AT24CXX_ReadOneByte(uint16_t ReadAddr);

/**
 * @brief 在 AT24CXX 指定地址写入一个字节
 * @param WriteAddr: 写入的地址 (0 ~ EE_TYPE)
 * @param DataToWrite: 要写入的数据
 */
void AT24CXX_WriteOneByte(uint16_t WriteAddr, uint8_t DataToWrite);

/**
 * @brief 在 AT24CXX 指定地址写入指定长度的数据
 * @param WriteAddr: 开始写入的地址
 * @param pBuffer: 数据缓冲区指针
 * @param NumToWrite: 要写入的字节数
 */
void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite);

/**
 * @brief 在 AT24CXX 指定地址读取指定长度的数据
 * @param ReadAddr: 开始读取的地址
 * @param pBuffer: 数据缓冲区指针
 * @param NumToRead: 要读取的字节数
 */
void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead);

/**
 * @brief 检查 AT24CXX 是否正常
 * @return 0: 正常, 1: 异常
 */
uint8_t AT24CXX_Check(void);

#endif //AT24CXX_H
