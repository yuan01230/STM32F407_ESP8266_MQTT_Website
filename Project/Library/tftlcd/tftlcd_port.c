#include "tftlcd_port.h"

/**
 * @brief LCD 端口层初始化
 * @details
 * 该函数不负责 GPIO/FSMC 外设配置，因为这些初始化已经由
 * CubeMX 生成代码完成。驱动层只在这里统一收口 LCD 上电后
 * 必须执行的板级动作，避免业务层直接操作 LCD 相关引脚。
 */
void TFTLCD_Port_Init(void)
{
    TFTLCD_Port_HardwareReset();
    TFTLCD_Port_SetBacklight(1U);
}

/**
 * @brief 写 LCD 复位引脚电平
 * @details
 * 当前板级电路中，TFTLCD 的 RESET 与系统 RESET 网络相连，
 * 没有单独接到 MCU GPIO，因此这里不再实际驱动任何引脚。
 */
void TFTLCD_Port_WriteResetPin(GPIO_PinState state)
{
    (void)state;
}

/**
 * @brief 控制 LCD 背光引脚
 * @details
 * 当前普中-麒麟 F407 开发板上，LCD 背光由 `LCD_BL` 直接控制，
 * 输出高电平时点亮背光，输出低电平时关闭背光。
 */
void TFTLCD_Port_SetBacklight(uint8_t enable)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port,
                      LCD_BL_Pin,
                      (enable != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief 执行 LCD 硬件复位时序
 * @details
 * 复位流程说明：
 * 1. 先确保复位脚处于释放状态，避免刚进入函数时状态不确定
 * 2. 拉低复位脚，让 LCD 控制器内部逻辑清零
 * 3. 保持足够时间，等待控制器内部状态稳定
 * 4. 再拉高复位脚，等待控制器重新启动
 *
 * 这一步只处理板级硬件状态，不涉及任何寄存器初始化。
 */
void TFTLCD_Port_HardwareReset(void)
{
    HAL_Delay(120);
}
