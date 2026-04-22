/**
 * @file    sram.h
 * @brief   IS62WV51216 SRAM 驱动程序头文件
 * @note    STM32F407 通过 FSMC 接口驱动外部 SRAM
 *          接口类型：FSMC 16 位并行接口
 *          片选 Bank：Bank1 Region3 (基地址 0x68000000)
 */

#ifndef __SRAM_H
#define __SRAM_H

#include "main.h"
#include <stdint.h>

/* ============================================================================
   引脚定义 - STM32F407 与 IS62WV51216 连接
   ============================================================================ */

/* --- 控制线 (Control Lines) --- */
#define SRAM_CS     PG10    // FSMC_NE3, 片选信号
#define SRAM_WE     PD5     // FSMC_NWE, 写使能
#define SRAM_OE     PD4     // FSMC_NOE, 读使能 (Output Enable)
#define SRAM_UB     PE1     // FSMC_NBL1, 高字节屏蔽 (Upper Byte)
#define SRAM_LB     PE0     // FSMC_NBL0, 低字节屏蔽 (Lower Byte)

/* --- 地址线 (Address Lines: A0~A18) --- */
#define SRAM_A0     PF0
#define SRAM_A1     PF1
#define SRAM_A2     PF2
#define SRAM_A3     PF3
#define SRAM_A4     PF4
#define SRAM_A5     PF5
#define SRAM_A6     PF12
#define SRAM_A7     PF13
#define SRAM_A8     PF14
#define SRAM_A9     PF15
#define SRAM_A10    PG0
#define SRAM_A11    PG1
#define SRAM_A12    PG2
#define SRAM_A13    PG3
#define SRAM_A14    PG4
#define SRAM_A15    PG5
#define SRAM_A16    PD11
#define SRAM_A17    PD12
#define SRAM_A18    PD13

/* --- 16 位数据线 (Data Lines: D0~D15) --- */
/* 注意：与 TFTLCD 共享数据总线 */
#define SRAM_D0     PD14
#define SRAM_D1     PD15
#define SRAM_D2     PD0
#define SRAM_D3     PD1
#define SRAM_D4     PE7
#define SRAM_D5     PE8
#define SRAM_D6     PE9
#define SRAM_D7     PE10
#define SRAM_D8     PE11
#define SRAM_D9     PE12
#define SRAM_D10    PE13
#define SRAM_D11    PE14
#define SRAM_D12    PE15
#define SRAM_D13    PD8
#define SRAM_D14    PD9
#define SRAM_D15    PD10

/* ============================================================================
   SRAM 地址定义
   ============================================================================ */

/* SRAM 基地址 - FSMC Bank1 Region3 */
#define SRAM_BASE_ADDR        ((uint32_t)0x68000000)

/* SRAM 容量定义 */
#define SRAM_SIZE_KB          1024        // 512K * 16-bit = 1024KB (1MB)
#define SRAM_SIZE_BYTES       (1024UL * 1024UL)

/* SRAM 地址范围 */
#define SRAM_START_ADDR       ((uint32_t)0x68000000)
#define SRAM_END_ADDR         ((uint32_t)(0x68000000 + SRAM_SIZE_BYTES - 1))

/* 将 SRAM 映射为 uint16_t 指针 (16 位访问) */
#define SRAM                  ((uint16_t *)SRAM_BASE_ADDR)

/* ============================================================================
   函数声明
   ============================================================================ */

/**
 * @brief 初始化 FSMC 以驱动 SRAM
 * @note  配置 FSMC Bank1 Region3，时序参数根据 IS62WV51216 规格书设置
 */
void SRAM_Init(void);

/**
 * @brief 向 SRAM 指定地址写入一个字节
 * @param addr: SRAM 地址 (0 ~ 524287)
 * @param data: 要写入的数据
 */
void SRAM_WriteByte(uint32_t addr, uint8_t data);

/**
 * @brief 从 SRAM 指定地址读取一个字节
 * @param addr: SRAM 地址 (0 ~ 524287)
 * @return 读取到的数据
 */
uint8_t SRAM_ReadByte(uint32_t addr);

/**
 * @brief 向 SRAM 指定地址写入一个 16 位字
 * @param addr: SRAM 地址 (0 ~ 262143)
 * @param data: 要写入的数据 (16 位)
 */
void SRAM_WriteHalfWord(uint32_t addr, uint16_t data);

/**
 * @brief 从 SRAM 指定地址读取一个 16 位字
 * @param addr: SRAM 地址 (0 ~ 262143)
 * @return 读取到的数据 (16 位)
 */
uint16_t SRAM_ReadHalfWord(uint32_t addr);

/**
 * @brief 向 SRAM 连续写入多个字节
 * @param addr: 起始地址
 * @param pBuf: 数据缓冲区指针
 * @param len: 要写入的字节数
 */
void SRAM_WriteBuffer(uint32_t addr, uint8_t *pBuf, uint32_t len);

/**
 * @brief 从 SRAM 连续读取多个字节
 * @param addr: 起始地址
 * @param pBuf: 数据缓冲区指针
 * @param len: 要读取的字节数
 */
void SRAM_ReadBuffer(uint32_t addr, uint8_t *pBuf, uint32_t len);

/**
 * @brief 填充 SRAM 指定区域
 * @param addr: 起始地址
 * @param data: 要填充的数据
 * @param len: 填充长度 (字节数)
 */
void SRAM_Fill(uint32_t addr, uint8_t data, uint32_t len);

/**
 * @brief SRAM 自检函数
 * @retval 0: 成功; 其他：失败
 */
uint8_t SRAM_SelfTest(void);

#endif /* __SRAM_H */
