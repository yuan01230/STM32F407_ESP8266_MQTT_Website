#include "beep.h"
#include "gpio_device.h"
#include "board_drv_config.h"

/*
 * 蜂鸣器驱动设计说明：
 * - 通过 BOARD_BEEP_CONFIG 静态绑定板级引脚与有效电平
 * - 通过 gpio_device_write/toggle 统一处理逻辑电平映射
 */
static const gpio_device_t s_beep = BOARD_BEEP_CONFIG;

void BEEP_Init(void)
{
    /* 初始化后默认关闭蜂鸣器，避免误鸣 */
    BEEP_Off();
}

void BEEP_On(void)
{
    /* 逻辑 1：打开蜂鸣器 */
    gpio_device_write(&s_beep, 1U);
}

void BEEP_Off(void)
{
    /* 逻辑 0：关闭蜂鸣器 */
    gpio_device_write(&s_beep, 0U);
}

void BEEP_Toggle(void)
{
    /* 翻转当前物理引脚电平 */
    gpio_device_toggle(&s_beep);
}

void BEEP_Set(BEEP_Status_t status)
{
    /* 根据目标状态统一写入逻辑电平 */
    gpio_device_write(&s_beep, (status == BEEP_ON) ? 1U : 0U);
}
