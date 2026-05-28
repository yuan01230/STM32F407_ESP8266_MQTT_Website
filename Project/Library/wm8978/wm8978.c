#include "wm8978.h"

#include <stdio.h>

#include "../SoftwareI2C/softwarei2c.h"

#define WM8978_REG_COUNT            58U
#define WM8978_DAC_VOL_MAX          255U

/* WM8978 寄存器多为只写，软件侧维护一份缓存，便于后续按位修改输出通道、
 * 音量和 I2S 格式。缓存值以 9bit 有效数据保存。
 */
static uint16_t wm8978_reg_cache[WM8978_REG_COUNT];
static uint8_t wm8978_active_addr = WM8978_I2C_ADDR_7BIT;

/**
 * @brief 读取 WM8978 软件寄存器缓存。
 * @param reg 寄存器编号，范围 0 到 57。
 * @return 缓存中的 9bit 寄存器值；寄存器编号非法时返回 0。
 * @note WM8978 多数寄存器不可读，因此通过软件缓存支持后续按位修改。
 */
static uint16_t WM8978_ReadCache(uint8_t reg)
{
    if (reg >= WM8978_REG_COUNT)
    {
        return 0U;
    }

    return wm8978_reg_cache[reg];
}

/**
 * @brief 等待 WM8978 写传输阶段 ACK。
 * @param 无。
 * @return 0 表示收到 ACK；非 0 表示 NACK。
 * @note WM8978_IGNORE_ACK_TEST 只用于早期排查，不应作为正常播放配置。
 */
static uint8_t WM8978_WaitACKForWrite(void)
{
    /* 正常运行必须检查 ACK。IGNORE_ACK 只保留给早期硬件排查，不建议打开。 */
    if (WM8978_IGNORE_ACK_TEST != 0U)
    {
        return SoftwareI2C_WaitACKNoStop();
    }

    return SoftwareI2C_WaitACK();
}

/**
 * @brief 探测指定 7bit IIC 地址是否有设备 ACK。
 * @param addr 7bit IIC 地址。
 * @return 0 表示地址阶段 ACK；非 0 表示 NACK。
 */
static uint8_t WM8978_ProbeAddress(uint8_t addr)
{
    uint8_t ack;

    /* 只发送器件地址，不写寄存器，用来确认 WM8978 控制口是否在线。 */
    SoftwareI2C_Start();
    SoftwareI2C_SendByte((uint8_t)(addr << 1));
    ack = SoftwareI2C_WaitACK();
    SoftwareI2C_Stop();

    return ack;
}

/**
 * @brief 探测当前板载 WM8978 地址。
 * @param 无。
 * @return 0 表示探测到 WM8978；1 表示多次重试后未收到 ACK。
 * @note 当前硬件实测使用 7bit 地址 0x1A。
 */
static uint8_t WM8978_DetectAddress(void)
{
    /* 当前普中麒麟 F407 板实测 WM8978 7bit 地址为 0x1A。 */
    for (uint8_t retry = 0U; retry < 5U; retry++)
    {
        uint8_t nack = WM8978_ProbeAddress(0x1AU);
        if (nack == 0U)
        {
            wm8978_active_addr = 0x1AU;
            printf("[WM8978] addr 0x1A ACK\r\n");
            return 0U;
        }
        HAL_Delay(2U);
    }

    printf("[WM8978] addr 0x1A not found\r\n");
    return 1U;
}

/**
 * @brief 获取当前驱动使用的 WM8978 7bit 地址。
 * @param 无。
 * @return 当前活动地址，正常为 0x1A。
 */
uint8_t WM8978_GetActiveAddress(void)
{
    return wm8978_active_addr;
}

/**
 * @brief 向 WM8978 写入一个 9bit 寄存器值。
 * @param reg WM8978 寄存器编号。
 * @param value 9bit 寄存器数据，函数内部只保留低 9bit。
 * @return 0 表示成功；1 表示寄存器编号非法；2/3/4 分别表示地址、寄存器高位、数据低位阶段 NACK。
 * @note 写入成功后会同步更新 wm8978_reg_cache。
 */
uint8_t WM8978_WriteReg(uint8_t reg, uint16_t value)
{
    uint8_t ack;
    uint8_t last_error = 0U;

    if (reg >= WM8978_REG_COUNT)
    {
        return 1U;
    }

    /* WM8978 控制协议：先发 7bit 寄存器地址 + 数据 bit8，再发数据低 8bit。 */
    value &= 0x01FFU;

    for (uint8_t retry = 0U; retry < 3U; retry++)
    {
        SoftwareI2C_Start();
        SoftwareI2C_SendByte((uint8_t)(wm8978_active_addr << 1));
        ack = WM8978_WaitACKForWrite();
        if ((ack != 0U) && (WM8978_IGNORE_ACK_TEST == 0U))
        {
            SoftwareI2C_Stop();
            last_error = 2U;
            HAL_Delay(1U);
            continue;
        }

        SoftwareI2C_SendByte((uint8_t)((reg << 1) | ((value >> 8) & 0x01U)));
        ack = WM8978_WaitACKForWrite();
        if ((ack != 0U) && (WM8978_IGNORE_ACK_TEST == 0U))
        {
            SoftwareI2C_Stop();
            last_error = 3U;
            HAL_Delay(1U);
            continue;
        }

        SoftwareI2C_SendByte((uint8_t)(value & 0xFFU));
        ack = WM8978_WaitACKForWrite();
        SoftwareI2C_Stop();
        if ((ack != 0U) && (WM8978_IGNORE_ACK_TEST == 0U))
        {
            last_error = 4U;
            HAL_Delay(1U);
            continue;
        }

        wm8978_reg_cache[reg] = value;
        return 0U;
    }

    printf("[WM8978] write reg %u val=0x%03X failed stage=%u\r\n",
           (unsigned int)reg,
           (unsigned int)value,
           (unsigned int)last_error);
    return last_error;
}

/**
 * @brief 设置 WM8978 DAC 数字音量。
 * @param volume DAC 音量，范围 0 到 255。
 * @return 0 表示成功；其他值为 WM8978_WriteReg 返回的错误码。
 * @note 当前音乐播放主要调节耳机/喇叭输出音量，DAC 音量保留为底层能力。
 */
uint8_t WM8978_SetDACVolume(uint8_t volume)
{
    uint8_t vol = volume;
    uint8_t ret;

    if (vol > WM8978_DAC_VOL_MAX)
    {
        vol = WM8978_DAC_VOL_MAX;
    }

    /* R11/R12 控制 DAC 左右声道音量，右声道写入 update 位让左右同时生效。 */
    ret = WM8978_WriteReg(11U, (uint16_t)vol);
    if (ret != 0U)
    {
        return ret;
    }

    return WM8978_WriteReg(12U, (uint16_t)(0x0100U | vol));
}

/**
 * @brief 设置耳机左右声道输出音量。
 * @param left 左声道音量，低 6bit 有效，范围 0 到 63。
 * @param right 右声道音量，低 6bit 有效，范围 0 到 63。
 * @return 0 表示成功；其他值为 WM8978_WriteReg 返回的错误码。
 * @note right 写入 update 位，使左右声道音量同步生效。
 */
uint8_t WM8978_SetHPVolume(uint8_t left, uint8_t right)
{
    uint8_t ret;

    /* 耳机音量为 6bit；音量为 0 时置 mute 位，避免残余噪声。 */
    left &= 0x3FU;
    right &= 0x3FU;
    if (left == 0U)
    {
        left |= (1U << 6);
    }
    if (right == 0U)
    {
        right |= (1U << 6);
    }

    ret = WM8978_WriteReg(52U, left);
    if (ret != 0U)
    {
        return ret;
    }

    return WM8978_WriteReg(53U, (uint16_t)(right | (1U << 8)));
}

/**
 * @brief 设置喇叭输出音量。
 * @param volume 喇叭音量，低 6bit 有效，范围 0 到 63。
 * @return 0 表示成功；其他值为 WM8978_WriteReg 返回的错误码。
 */
uint8_t WM8978_SetSPKVolume(uint8_t volume)
{
    /* 喇叭输出和耳机输出分开设置，当前播放流程两者都同步更新。 */
    uint8_t vol = (uint8_t)(volume & 0x3FU);
    uint8_t ret;

    if (vol == 0U)
    {
        vol |= (1U << 6);
    }

    ret = WM8978_WriteReg(54U, vol);
    if (ret != 0U)
    {
        return ret;
    }

    return WM8978_WriteReg(55U, (uint16_t)(vol | (1U << 8)));
}

/**
 * @brief 配置 WM8978 音频接口格式。
 * @param format 音频数据格式，当前使用 2 表示 I2S Philips。
 * @param length 数据位宽编码，当前使用 0 表示 16bit。
 * @return 0 表示成功；其他值为 WM8978_WriteReg 返回的错误码。
 */
uint8_t WM8978_ConfigI2S(uint8_t format, uint8_t length)
{
    /* format=2 对应 I2S Philips，length=0 对应 16bit 数据长度。 */
    format &= 0x03U;
    length &= 0x03U;
    return WM8978_WriteReg(4U, (uint16_t)((format << 3) | (length << 5)));
}

/**
 * @brief 打开 WM8978 播放输出路径。
 * @param 无。
 * @return 0 表示成功；其他值为 WM8978_WriteReg 返回的错误码。
 * @note 该函数打开 DAC、输出混音和耳机/喇叭输出，并关闭不需要的输入旁路。
 */
uint8_t WM8978_EnablePlayback(void)
{
    uint16_t regval;
    uint8_t ret;

    /* 打开 DAC/输出混音/耳机输出路径，关闭不需要的输入旁路，减少噪声来源。 */
    regval = WM8978_ReadCache(3U);
    regval |= 3U;
    ret = WM8978_WriteReg(3U, regval);
    if (ret != 0U) return ret;

    regval = WM8978_ReadCache(2U);
    regval &= (uint16_t)~3U;
    ret = WM8978_WriteReg(2U, regval);
    if (ret != 0U) return ret;

    ret = WM8978_WriteReg(44U, 0U);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(47U, (uint16_t)(WM8978_ReadCache(47U) & (uint16_t)~0x017FU));
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(48U, (uint16_t)(WM8978_ReadCache(48U) & (uint16_t)~0x017FU));
    if (ret != 0U) return ret;

    ret = WM8978_WriteReg(50U, 0x001U);
    if (ret != 0U) return ret;
    return WM8978_WriteReg(51U, 0x001U);
}

/**
 * @brief 初始化 WM8978 到当前工程所需的播放状态。
 * @param 无。
 * @return 0 表示成功；2 表示地址探测失败；其他值为寄存器写入错误码。
 * @note WAV、MP3、tone 播放前都会调用该函数，确保音频芯片处于一致状态。
 */
uint8_t WM8978_Init(void)
{
    uint8_t ret;

    /* 初始化顺序：软件 IIC -> 地址探测 -> 软件复位 -> 电源/音频路径/I2S/音量配置。 */
    SoftwareI2C_Init();
    SoftwareI2C_SetDelay(5U);
    SoftwareI2C_Stop();
    HAL_Delay(100U);
    ret = WM8978_DetectAddress();
    if ((ret != 0U) && (WM8978_IGNORE_ACK_TEST == 0U))
    {
        return 2U;
    }
    if (ret != 0U)
    {
        wm8978_active_addr = WM8978_I2C_ADDR_7BIT;
        printf("[WM8978] no ACK, force write addr 0x%02X for test\r\n", wm8978_active_addr);
    }

    /* R0 软件复位后延时，保证后续电源管理寄存器写入稳定。 */
    ret = WM8978_WriteReg(0U, 0x000U);
    if (ret != 0U)
    {
        return ret;
    }
    HAL_Delay(20U);

    ret = WM8978_WriteReg(1U, 0x01BU);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(2U, 0x1B0U);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(3U, 0x06CU);
    if (ret != 0U) return ret;

    ret = WM8978_WriteReg(6U, 0x000U);
    if (ret != 0U) return ret;

    ret = WM8978_WriteReg(43U, 1U << 4);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(47U, 1U << 8);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(48U, 1U << 8);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(49U, 1U << 1);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(10U, 1U << 3);
    if (ret != 0U) return ret;
    ret = WM8978_WriteReg(14U, 1U << 3);
    if (ret != 0U) return ret;

    ret = WM8978_SetHPVolume(40U, 40U);
    if (ret != 0U) return ret;
    ret = WM8978_SetSPKVolume(50U);
    if (ret != 0U) return ret;
    ret = WM8978_ConfigI2S(2U, 0U);
    if (ret != 0U) return ret;
    return WM8978_EnablePlayback();
}
