/**
 * @file    flash.h
 * @brief   STM32F407ZGT6 内部 Flash 读写驱动
 */

#ifndef FLASH_H
#define FLASH_H

#include "main.h"

/* Flash 起始地址 */
#define STM32_FLASH_BASE 0x08000000 	// STM32 FLASH 的起始地址
#define STM32_FLASH_SIZE 0x100000  	    // STM32F407ZGT6 的 FLASH 大小 (1MB)
#define STM32_FLASH_END  (STM32_FLASH_BASE + STM32_FLASH_SIZE)

/* Flash 扇区定义 (F407 1MB 规格) */
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Sector 0,  16 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Sector 1,  16 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Sector 2,  16 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Sector 3,  16 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Sector 4,  64 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Sector 5,  128 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Sector 6,  128 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Sector 7,  128 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Sector 8,  128 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Sector 9,  128 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Sector 11, 128 Kbytes */

/* 函数声明 */
uint32_t STM32FLASH_ReadWord(uint32_t faddr);                      // 读取一个字(32位)
void STM32FLASH_Write(uint32_t WriteAddr, uint32_t *pBuffer, uint32_t NumToWrite); // 写入数据(带擦除)
void STM32FLASH_WriteNoErase(uint32_t WriteAddr, uint32_t *pBuffer, uint32_t NumToWrite); // 写入数据(不带擦除)
void STM32FLASH_Read(uint32_t ReadAddr, uint32_t *pBuffer, uint32_t NumToRead);    // 读取数据
void STM32FLASH_Erase(uint32_t StartAddr, uint32_t NumBytes);      // 擦除指定地址范围所在的扇区

#endif //FLASH_H
