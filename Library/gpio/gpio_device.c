#include "gpio_device.h"

/**
 * @brief 按逻辑状态控制 GPIO 设备
 * @param dev GPIO 设备描述
 * @param logical_on 逻辑状态（0=关，非0=开）
 */
void gpio_device_write(const gpio_device_t *dev, uint8_t logical_on)
{
    /* 参数防护：避免空指针导致异常访问 */
    if (dev == NULL || dev->port == NULL) {
        return;
    }

    /* 根据设备有效电平定义，换算为物理电平 */
    GPIO_PinState pin_state;
    if (dev->active_level == GPIO_ACTIVE_HIGH) {
        pin_state = logical_on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        pin_state = logical_on ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(dev->port, dev->pin, pin_state);
}

/**
 * @brief 读取 GPIO 设备当前逻辑状态
 * @param dev GPIO 设备描述
 * @return uint8_t 1=逻辑有效，0=逻辑无效
 */
uint8_t gpio_device_read(const gpio_device_t *dev)
{
    /* 参数防护：约定空设备返回无效状态 */
    if (dev == NULL || dev->port == NULL) {
        return 0U;
    }

    /* 读取当前物理引脚电平 */
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(dev->port, dev->pin);

    /* 根据有效电平定义，映射为统一逻辑值 */
    if (dev->active_level == GPIO_ACTIVE_HIGH) {
        return (pin_state == GPIO_PIN_SET) ? 1U : 0U;
    }

    return (pin_state == GPIO_PIN_RESET) ? 1U : 0U;
}

/**
 * @brief 翻转 GPIO 设备物理电平
 * @param dev GPIO 设备描述
 */
void gpio_device_toggle(const gpio_device_t *dev)
{
    /* 参数防护：避免空指针访问 */
    if (dev == NULL || dev->port == NULL) {
        return;
    }

    HAL_GPIO_TogglePin(dev->port, dev->pin);
}
