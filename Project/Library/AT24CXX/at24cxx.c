//
// Created by 标语 on 26-3-7.
//

#include "at24cxx.h"
#include "../SoftwareI2C/softwarei2c.h"

/**
 * @brief 初始化 AT24CXX
 */
void AT24CXX_Init(void)
{
    SoftwareI2C_Init();
}

/**
 * @brief 在 AT24CXX 指定地址读取一个字节
 * @param ReadAddr: 开始读取的地址
 * @return 读取到的数据
 */
uint8_t AT24CXX_ReadOneByte(uint16_t ReadAddr)
{
    uint8_t temp = 0;

    SoftwareI2C_Start();

    // 1. 发送设备地址 (写模式)
    if (EE_TYPE > AT24C16)
    {
        SoftwareI2C_SendByte(AT24CXX_ADDR);
        SoftwareI2C_WaitACK();
        SoftwareI2C_SendByte(ReadAddr >> 8);   // 发送高地址
        SoftwareI2C_WaitACK();
        SoftwareI2C_SendByte(ReadAddr & 0xFF); // 发送低地址
        SoftwareI2C_WaitACK();
    }
    else
    {
        // 对于 AT24C04/08/16，地址的高位需要放到设备地址中
        SoftwareI2C_SendByte(AT24CXX_ADDR + ((ReadAddr / 256) << 1));
        SoftwareI2C_WaitACK();
        SoftwareI2C_SendByte(ReadAddr % 256);  // 发送低地址
        SoftwareI2C_WaitACK();
    }

    // 2. 重新产生起始信号
    SoftwareI2C_Start();

    // 3. 发送设备地址 (读模式)
    if (EE_TYPE > AT24C16)
    {
        SoftwareI2C_SendByte(AT24CXX_ADDR | 0x01);
    }
    else
    {
        SoftwareI2C_SendByte((AT24CXX_ADDR + ((ReadAddr / 256) << 1)) | 0x01);
    }
    SoftwareI2C_WaitACK();

    // 4. 读取数据
    temp = SoftwareI2C_ReceiveByte();

    // 5. 发送 NACK (读完最后一个字节发送 NACK)
    SoftwareI2C_SendACK(1);

    SoftwareI2C_Stop();

    return temp;
}

/**
 * @brief 在 AT24CXX 指定地址写入一个字节
 * @param WriteAddr: 写入的地址
 * @param DataToWrite: 要写入的数据
 */
void AT24CXX_WriteOneByte(uint16_t WriteAddr, uint8_t DataToWrite)
{
    SoftwareI2C_Start();

    // 1. 发送设备地址 (写模式)
    if (EE_TYPE > AT24C16)
    {
        SoftwareI2C_SendByte(AT24CXX_ADDR);
        SoftwareI2C_WaitACK();
        SoftwareI2C_SendByte(WriteAddr >> 8);   // 发送高地址
        SoftwareI2C_WaitACK();
        SoftwareI2C_SendByte(WriteAddr & 0xFF); // 发送低地址
        SoftwareI2C_WaitACK();
    }
    else
    {
        SoftwareI2C_SendByte(AT24CXX_ADDR + ((WriteAddr / 256) << 1));
        SoftwareI2C_WaitACK();
        SoftwareI2C_SendByte(WriteAddr % 256);  // 发送低地址
        SoftwareI2C_WaitACK();
    }

    // 2. 发送数据
    SoftwareI2C_SendByte(DataToWrite);
    SoftwareI2C_WaitACK();

    SoftwareI2C_Stop();

    // 3. 等待写入完成 (EEPROM 内部写入需要时间，通常 5-10ms)
    HAL_Delay(10);
}

/**
 * @brief 在 AT24CXX 指定地址写入指定长度的数据
 * @note  该函数未处理页写入限制，如果跨页写入可能会回卷。
 *        为了简单起见，这里循环调用 WriteOneByte。
 *        虽然效率低，但最稳健。
 */
void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite)
{
    while (NumToWrite--)
    {
        AT24CXX_WriteOneByte(WriteAddr, *pBuffer);
        WriteAddr++;
        pBuffer++;
    }
}

/**
 * @brief 在 AT24CXX 指定地址读取指定长度的数据
 */
void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead)
{
    while (NumToRead--)
    {
        *pBuffer = AT24CXX_ReadOneByte(ReadAddr);
        ReadAddr++;
        pBuffer++;
    }
}

/**
 * @brief 检查 AT24CXX 是否正常
 * @return 0: 正常, 1: 异常
 * @note   在最后一个地址写入 0x55，读取并比对，最后恢复原数据。
 */
uint8_t AT24CXX_Check(void)
{
    uint8_t temp;
    uint8_t original_data;

    // 1. 读取并保存原始数据
    original_data = AT24CXX_ReadOneByte(EE_TYPE);

    // 2. 写入测试数据 0x55
    AT24CXX_WriteOneByte(EE_TYPE, 0x55);

    // 3. 读取测试数据
    temp = AT24CXX_ReadOneByte(EE_TYPE);

    // 4. 恢复原始数据
    AT24CXX_WriteOneByte(EE_TYPE, original_data);

    // 5. 比对结果
    if (temp == 0x55)
    {
        return 0; // 正常
    }
    else
    {
        return 1; // 异常
    }
}
