#ifndef TFTLCD_PORT_H
#define TFTLCD_PORT_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    volatile uint16_t LCD_CMD;
    volatile uint16_t LCD_DATA;
} TFTLCD_PortTypeDef;

/**
 * @brief LCD 默认显示方向
 * @note
 * - `0U`：竖屏
 * - `1U`：横屏
 *
 * 当前工程保持和原始驱动一致，默认上电后使用竖屏模式。
 */
#define TFTLCD_DEFAULT_DIR 0U

/**
 * @brief LCD FSMC 映射基地址
 * @details
 * 当前开发板 LCD 通过 FSMC/NOR-SRAM 方式访问。
 * 原工程使用 A6 地址线区分命令区与数据区，因此沿用：
 * `0x6C000000 | 0x0000007E`
 * 这个映射地址，不改动既有硬件访问方案。
 */
#define TFTLCD_PORT_BASE_ADDR ((uint32_t)(0x6C000000UL | 0x0000007EUL))

/**
 * @brief LCD FSMC 映射访问指针
 * @note
 * - `TFTLCD_PORT->LCD_CMD`：访问命令寄存器
 * - `TFTLCD_PORT->LCD_DATA`：访问数据寄存器
 */
#define TFTLCD_PORT ((TFTLCD_PortTypeDef *)TFTLCD_PORT_BASE_ADDR)

/**
 * @brief LCD 端口层初始化
 * @details
 * 当前工程的 GPIO 和 FSMC 初始化由 CubeMX 生成代码负责，
 * 因此端口层初始化不重复做外设配置，只统一完成：
 * 1. LCD 硬件复位
 * 2. 背光打开
 */
void TFTLCD_Port_Init(void);

/**
 * @brief 控制 LCD 硬件复位引脚
 * @param state 引脚输出状态
 * @note
 * - `GPIO_PIN_RESET`：拉低复位
 * - `GPIO_PIN_SET`：释放复位
 */
void TFTLCD_Port_WriteResetPin(GPIO_PinState state);

/**
 * @brief 控制 LCD 背光开关
 * @param enable `1` 打开背光，`0` 关闭背光
 */
void TFTLCD_Port_SetBacklight(uint8_t enable);

/**
 * @brief 执行 LCD 硬件复位时序
 * @details
 * 上电后先执行一次硬件复位，可以确保控制器回到已知状态，
 * 避免 LCD 因上电抖动、热启动或复位不完整导致初始化异常。
 */
void TFTLCD_Port_HardwareReset(void);

#ifdef __cplusplus
}
#endif

#endif
