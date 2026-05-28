/**
 * @file    flash.c
 * @brief   STM32F407ZGT6 内部 Flash 读写驱动实现
 */

#include "flash.h"

/**
 * @brief  读取指定地址的一个字(32位数据)
 * @param  faddr: 读取地址
 * @retval 读取到的数据
 */
uint32_t STM32FLASH_ReadWord(uint32_t faddr)
{
    return *(__IO uint32_t *)faddr;
}

/**
 * @brief  获取某个地址所在的 Flash 扇区
 * @param  addr: Flash 地址
 * @retval 扇区编号 (FLASH_SECTOR_0 ~ FLASH_SECTOR_11)
 */
static uint16_t STM32FLASH_GetSector(uint32_t addr)
{
    if (addr < ADDR_FLASH_SECTOR_1) return FLASH_SECTOR_0;
    else if (addr < ADDR_FLASH_SECTOR_2) return FLASH_SECTOR_1;
    else if (addr < ADDR_FLASH_SECTOR_3) return FLASH_SECTOR_2;
    else if (addr < ADDR_FLASH_SECTOR_4) return FLASH_SECTOR_3;
    else if (addr < ADDR_FLASH_SECTOR_5) return FLASH_SECTOR_4;
    else if (addr < ADDR_FLASH_SECTOR_6) return FLASH_SECTOR_5;
    else if (addr < ADDR_FLASH_SECTOR_7) return FLASH_SECTOR_6;
    else if (addr < ADDR_FLASH_SECTOR_8) return FLASH_SECTOR_7;
    else if (addr < ADDR_FLASH_SECTOR_9) return FLASH_SECTOR_8;
    else if (addr < ADDR_FLASH_SECTOR_10) return FLASH_SECTOR_9;
    else if (addr < ADDR_FLASH_SECTOR_11) return FLASH_SECTOR_10;
    return FLASH_SECTOR_11;
}

/**
 * @brief  擦除指定地址范围所在的扇区
 * @param  StartAddr: 起始地址
 * @param  NumBytes: 字节数
 */
void STM32FLASH_Erase(uint32_t StartAddr, uint32_t NumBytes)
{
    FLASH_EraseInitTypeDef FlashEraseInit;
    uint32_t SectorError = 0;
    uint32_t FirstSector = 0, NbOfSectors = 0;
    uint32_t EndAddr = 0;

    if (StartAddr < STM32_FLASH_BASE || StartAddr >= STM32_FLASH_END) return;

    HAL_FLASH_Unlock();             // 解锁 Flash

    EndAddr = StartAddr + NumBytes;
    
    // 获取起始扇区和结束扇区，计算需要擦除的扇区数
    FirstSector = STM32FLASH_GetSector(StartAddr);
    NbOfSectors = STM32FLASH_GetSector(EndAddr - 1) - FirstSector + 1;

    // 擦除扇区配置
    FlashEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;     // 扇区擦除
    FlashEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;    // 电压范围 2.7V - 3.6V
    FlashEraseInit.Sector = FirstSector;
    FlashEraseInit.NbSectors = NbOfSectors;

    if (HAL_FLASHEx_Erase(&FlashEraseInit, &SectorError) != HAL_OK)
    {
        // 擦除错误处理 (可选: 添加错误上报)
    }

    HAL_FLASH_Lock();               // 锁定 Flash
}

/**
 * @brief  向 Flash 写入数据 (不带擦除)
 * @note   写入前请确保目标地址已擦除 (内容为 0xFFFFFFFF)
 * @param  WriteAddr: 写入起始地址 (必须 4 字节对齐)
 * @param  pBuffer: 数据缓冲区
 * @param  NumToWrite: 写入的字数 (32位数据的个数)
 */
void STM32FLASH_WriteNoErase(uint32_t WriteAddr, uint32_t *pBuffer, uint32_t NumToWrite)
{
    uint32_t EndAddr;

    if (WriteAddr < STM32_FLASH_BASE || WriteAddr >= STM32_FLASH_END) return;

    HAL_FLASH_Unlock();             // 解锁 Flash

    EndAddr = WriteAddr + (NumToWrite * 4);

    // 写入数据
    while (WriteAddr < EndAddr)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, WriteAddr, *pBuffer) != HAL_OK)
        {
            break;
        }
        WriteAddr += 4;
        pBuffer++;
    }

    HAL_FLASH_Lock();               // 锁定 Flash
}

/**
 * @brief  向 Flash 写入数据 (自动擦除对应扇区)
 * @note   此函数会先擦除涉及的所有扇区，然后写入数据。
 *         注意：这会清除扇区内的所有其他数据！
 * @param  WriteAddr: 写入起始地址 (必须 4 字节对齐)
 * @param  pBuffer: 数据缓冲区
 * @param  NumToWrite: 写入的字数 (32位数据的个数)
 */
void STM32FLASH_Write(uint32_t WriteAddr, uint32_t *pBuffer, uint32_t NumToWrite)
{
    // 复用已实现的擦除和写入函数
    // 注意: 这种简单复用会擦除整个扇区，如果只写扇区的一部分，
    // 扇区内其他未被新数据覆盖的区域将会丢失(变为0xFF)。
    // 如果需要保留扇区其他数据，需要先读出->修改->写入。
    
    // 这里保持原有逻辑：擦除涉及的扇区，然后写入。
    STM32FLASH_Erase(WriteAddr, NumToWrite * 4);
    STM32FLASH_WriteNoErase(WriteAddr, pBuffer, NumToWrite);
}

/**
 * @brief  从 Flash 读取指定长度的数据
 * @param  ReadAddr: 读取起始地址
 * @param  pBuffer: 存储缓冲区
 * @param  NumToRead: 读取的字数 (32位数据的个数)
 */
void STM32FLASH_Read(uint32_t ReadAddr, uint32_t *pBuffer, uint32_t NumToRead)
{
    uint32_t i;
    for (i = 0; i < NumToRead; i++)
    {
        pBuffer[i] = STM32FLASH_ReadWord(ReadAddr);
        ReadAddr += 4;
    }
}