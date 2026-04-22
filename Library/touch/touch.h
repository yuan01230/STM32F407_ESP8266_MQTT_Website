/**
 ******************************************************************************
 * @file    touch.h
 * @author  普中科技
 * @brief   XPT2046 电阻触摸屏驱动头文件
 *          适用于 STM32F407 + XPT2046 电阻屏组合
 *          
 * @硬件连接:
 *          T_CS (片选)     -> PC13
 *          T_SCK (时钟)    -> PB0
 *          T_MOSI (数据入) -> PF11
 *          T_MISO (数据出) -> PB2
 *          T_PEN (中断)    -> PB1
 *          
 * @版本    V2.0
 * @日期    2026-03-17
 ******************************************************************************
 */

#ifndef __TOUCH_H
#define __TOUCH_H

#include "main.h"

/* ============================================================================
 * 类型定义 (兼容老代码)
 * ============================================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* ============================================================================
 * 硬件引脚定义 (基于普中 - 麒麟 F407 开发板)
 * ============================================================================ */

/* 1. T_CS (触摸片选) - PC13 */
#define TOUCH_CS_PORT       GPIOC
#define TOUCH_CS_PIN        GPIO_PIN_13

/* 2. T_SCK / T_CLK (SPI 时钟) - PB0 */
#define TOUCH_SCK_PORT      GPIOB
#define TOUCH_SCK_PIN       GPIO_PIN_0

/* 3. T_MOSI / T_DIN (数据输入) - PF11 */
#define TOUCH_MOSI_PORT     GPIOF
#define TOUCH_MOSI_PIN      GPIO_PIN_11

/* 4. T_MISO / T_DOUT (数据输出) - PB2 */
#define TOUCH_MISO_PORT     GPIOB
#define TOUCH_MISO_PIN      GPIO_PIN_2

/* 5. T_PEN / IRQ (中断请求) - PB1 */
#define TOUCH_PEN_PORT      GPIOB
#define TOUCH_PEN_PIN       GPIO_PIN_1

/* ============================================================================
 * 触摸状态标志位定义
 * ============================================================================ */
#define TP_PRES_DOWN    0x80    /*!< 触屏被按下标志位 (bit7) */
#define TP_CATH_PRES    0x40    /*!< 有按键按下了标志位 (bit6) */
#define CT_MAX_TOUCH    10      /*!< 最多支持的触摸点数 (电容屏用，电阻屏主要用第 0 个) */

/* ============================================================================
 * 校准参数存储地址定义 (AT24CXX EEPROM)
 * ============================================================================ */
#define TP_SAVE_ADDR_BASE   40  /*!< EEPROM 保存校准参数的起始地址 */
#define TP_SAVE_FLAG        0x0A /*!< 校准标志值 */

/* ============================================================================
 * 触摸屏控制器数据结构
 * ============================================================================ */
typedef struct
{
    /* 函数指针成员 */
    u8 (*init)(void);           /*!< 初始化触摸屏控制器 */
    u8 (*scan)(u8);             /*!< 扫描触摸屏：0-屏幕坐标扫描，1-物理坐标扫描 */
    void (*adjust)(void);       /*!< 触摸屏校准函数 */
    
    /* 坐标数据 */
    u16 x[CT_MAX_TOUCH];        /*!< 当前触摸点 X 坐标数组 */
    u16 y[CT_MAX_TOUCH];        /*!< 当前触摸点 Y 坐标数组 */
    
    /* 状态寄存器 */
    /* bit7: 0-没有按下; 1-有按下
     * bit6: 0-没有按键按下; 1-有按键按下
     * bit5: 保留
     * bit4~bit0: 电容触摸屏按下的点数 (0 表示没有按下，1 表示按下)
     */
    u16 sta;                    /*!< 触摸状态寄存器 */
    
    /* 校准参数 - 用于将物理坐标转换为屏幕坐标 */
    float xfac;                 /*!< X 轴比例因子 */
    float yfac;                 /*!< Y 轴比例因子 */
    short xoff;                 /*!< X 轴偏移量 */
    short yoff;                 /*!< Y 轴偏移量 */
    
    /* 触摸屏类型标识 */
    /* 0: 未知类型
     * 1: 电阻屏 (XPT2046)
     * 2: 电容屏
     */
    u8 touchtype;               /*!< 触摸屏类型 */
} _m_tp_dev;

extern _m_tp_dev tp_dev;

/* ============================================================================
 * HAL 库引脚操作宏定义
 * ============================================================================ */
#define PEN         HAL_GPIO_ReadPin(TOUCH_PEN_PORT, TOUCH_PEN_PIN)                     /*!< 读取触摸笔按下检测引脚 */
#define DOUT        HAL_GPIO_ReadPin(TOUCH_MISO_PORT, TOUCH_MISO_PIN)                   /*!< 读取 SPI 数据输出引脚 */
#define TDIN(n)     HAL_GPIO_WritePin(TOUCH_MOSI_PORT, TOUCH_MOSI_PIN, (n)?GPIO_PIN_SET:GPIO_PIN_RESET) /*!< 设置 SPI 数据输入 */
#define TCLK(n)     HAL_GPIO_WritePin(TOUCH_SCK_PORT, TOUCH_SCK_PIN, (n)?GPIO_PIN_SET:GPIO_PIN_RESET) /*!< 设置 SPI 时钟 */
#define TCS(n)      HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, (n)?GPIO_PIN_SET:GPIO_PIN_RESET)   /*!< 设置片选信号 */

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/* SPI 底层驱动函数 */
void TP_Write_Byte(u8 num);                             /*!< SPI 写一个字节 */
u16 TP_Read_AD(u8 CMD);                                 /*!< SPI 读 AD 转换值 (带命令) */

/* 坐标读取函数 */
u16 TP_Read_XOY(u8 xy);                                 /*!< 读取 X 或 Y 的物理坐标 */
u8 TP_Read_XY(u16 *x, u16 *y);                          /*!< 读取一次 X 和 Y 坐标 */
u8 TP_Read_XY2(u16 *x, u16 *y);                         /*!< 连续读取两次并校验，提高精度 */
void Touch_Get_Raw_XY(uint16_t *x, uint16_t *y);        /*!< 获取原始 ADC 坐标 (供调试用) */

/* 校准相关函数 */
void TP_Save_Adjdata(void);                             /*!< 保存校准参数到 EEPROM */
u8 TP_Get_Adjdata(void);                                /*!< 从 EEPROM 获取校准参数 */
void TP_Adjust(void);                                   /*!< 执行触摸屏校准程序 */
void TP_Drow_Touch_Point(u16 x, u16 y, u16 color);      /*!< 画十字校准点 */
void TP_Draw_Big_Point(u16 x, u16 y, u16 color);        /*!< 画大圆点 */

/* 主接口函数 */
u8 TP_Scan(u8 tp);                                      /*!< 扫描触摸屏主函数 */
u8 TP_Init(void);                                       /*!< 触摸屏初始化入口 */

#endif /* __TOUCH_H */
