#ifndef GPIO_DEVICE_H
#define GPIO_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief GPIO 逻辑有效电平定义
 * @note
 * - GPIO_ACTIVE_HIGH：逻辑 1 映射为物理高电平（GPIO_PIN_SET）
 * - GPIO_ACTIVE_LOW ：逻辑 1 映射为物理低电平（GPIO_PIN_RESET）
 */
typedef enum {
    GPIO_ACTIVE_LOW = 0,
    GPIO_ACTIVE_HIGH = 1
} gpio_active_level_t;

/**
 * @brief 通用 GPIO 设备描述结构体
 * @note
 * - 用于描述一个可被“逻辑状态”控制或读取的 GPIO 外设
 * - 典型设备：LED、BEEP、KEY 等
 */
typedef struct {
    GPIO_TypeDef *port;               /**< GPIO 端口，例如 GPIOA/GPIOF */
    uint16_t pin;                     /**< GPIO 引脚掩码，例如 GPIO_PIN_8 */
    gpio_active_level_t active_level; /**< 逻辑有效电平定义 */
} gpio_device_t;

/**
 * @brief 按逻辑状态写 GPIO 设备
 * @param dev GPIO 设备描述指针
 * @param logical_on 逻辑状态：0=关闭/无效，非0=打开/有效
 * @note
 * - 函数内部会根据 `active_level` 自动完成逻辑到物理电平的换算
 * - 当 `dev==NULL` 或 `dev->port==NULL` 时直接返回，不执行写操作
 */
void gpio_device_write(const gpio_device_t *dev, uint8_t logical_on);

/**
 * @brief 读取 GPIO 设备逻辑状态
 * @param dev GPIO 设备描述指针
 * @return uint8_t 逻辑状态：1=有效，0=无效
 * @note
 * - 函数内部会根据 `active_level` 将物理电平映射为统一逻辑值
 * - 当 `dev==NULL` 或 `dev->port==NULL` 时返回 0
 */
uint8_t gpio_device_read(const gpio_device_t *dev);

/**
 * @brief 翻转 GPIO 设备当前物理电平
 * @param dev GPIO 设备描述指针
 * @note
 * - 翻转的是“物理引脚电平”，而非直接翻转逻辑状态标志
 * - 当 `dev==NULL` 或 `dev->port==NULL` 时直接返回
 */
void gpio_device_toggle(const gpio_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif
