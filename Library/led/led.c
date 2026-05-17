#include "led.h"
#include "gpio_device.h"
#include "board_drv_config.h"

/*
 * LED 驱动设计说明：
 * 1) 使用静态配置表注册 LED 设备（端口、引脚、有效电平）。
 * 2) 驱动逻辑与具体板卡引脚解耦，换板仅需改 board_drv_config.h。
 * 3) 统一通过 gpio_device_* 接口完成电平控制，避免重复代码。
 */
static const gpio_device_t s_led_table[] = {
    BOARD_LED_CONFIG_TABLE
};

/**
 * @brief 校验 LED 编号是否可用
 * @param led LED 枚举值
 * @return uint8_t 1=有效，0=无效
 * @note
 * - 同时检查 `LED_TypeDef` 范围和板级配置数量，防止数组越界。
 */
static uint8_t led_is_valid(LED_TypeDef led)
{
    return (led >= 0) && ((uint32_t)led < (uint32_t)LED_NUM) && ((uint32_t)led < BOARD_LED_COUNT);
}

/**
 * @brief 初始化 LED 驱动
 * @note 当前策略为默认熄灭所有 LED，避免上电后状态不确定。
 */
void LED_Init(void)
{
    LED_AllOff();
}

/**
 * @brief 点亮指定 LED
 * @param led LED 编号
 */
void LED_On(LED_TypeDef led)
{
    /* 无效编号直接返回，保持接口“安全无副作用” */
    if (!led_is_valid(led)) {
        return;
    }

    /* 逻辑 1 = 点亮；底层自动换算为物理高/低电平 */
    gpio_device_write(&s_led_table[led], 1U);
}

/**
 * @brief 熄灭指定 LED
 * @param led LED 编号
 */
void LED_Off(LED_TypeDef led)
{
    if (!led_is_valid(led)) {
        return;
    }

    /* 逻辑 0 = 熄灭；底层自动换算为物理高/低电平 */
    gpio_device_write(&s_led_table[led], 0U);
}

/**
 * @brief 翻转指定 LED 状态
 * @param led LED 编号
 */
void LED_Toggle(LED_TypeDef led)
{
    if (!led_is_valid(led)) {
        return;
    }

    gpio_device_toggle(&s_led_table[led]);
}

/**
 * @brief 点亮所有 LED
 */
void LED_AllOn(void)
{
    /* 双上限保护：防止枚举数量与配置数量不一致导致越界 */
    for (uint32_t i = 0; i < BOARD_LED_COUNT && i < (uint32_t)LED_NUM; ++i) {
        gpio_device_write(&s_led_table[i], 1U);
    }
}

/**
 * @brief 熄灭所有 LED
 */
void LED_AllOff(void)
{
    for (uint32_t i = 0; i < BOARD_LED_COUNT && i < (uint32_t)LED_NUM; ++i) {
        gpio_device_write(&s_led_table[i], 0U);
    }
}

/**
 * @brief 阻塞闪烁指定 LED
 * @param led LED 编号
 * @param period_ms 周期（毫秒）
 * @param times 闪烁次数
 */
void LED_Blink(LED_TypeDef led, uint32_t period_ms, uint8_t times)
{
    /* 参数保护：非法输入不执行动作 */
    if (!led_is_valid(led) || period_ms == 0U || times == 0U) {
        return;
    }

    for (uint8_t i = 0; i < times; ++i) {
        LED_On(led);
        HAL_Delay(period_ms / 2U);

        LED_Off(led);
        HAL_Delay(period_ms / 2U);
    }
}
