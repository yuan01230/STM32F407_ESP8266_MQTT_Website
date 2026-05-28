#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief LED 编号定义
 * @note
 * - 枚举顺序必须与板级配置表 `BOARD_LED_CONFIG_TABLE` 顺序一致。
 * - 若新增 LED，请同时修改：
 *   1) `LED_TypeDef` 枚举
 *   2) `BOARD_LED_COUNT`
 *   3) `BOARD_LED_CONFIG_TABLE`
 */
typedef enum {
    LED0 = 0, /**< 第 0 个 LED */
    LED1,     /**< 第 1 个 LED */
    LED_NUM   /**< LED 枚举数量上限（非真实硬件数量） */
} LED_TypeDef;

/**
 * @brief 初始化 LED 驱动
 * @note
 * - 该函数不负责 GPIO 外设初始化，GPIO 需由 `MX_GPIO_Init()` 提前完成。
 * - 当前初始化策略为“全部熄灭”。
 */
void LED_Init(void);

/**
 * @brief 点亮指定 LED
 * @param led LED 编号
 * @note
 * - 若编号非法，函数直接返回，不执行操作。
 * - 点亮逻辑会自动适配高/低电平有效，无需上层关心极性。
 */
void LED_On(LED_TypeDef led);

/**
 * @brief 熄灭指定 LED
 * @param led LED 编号
 * @note
 * - 若编号非法，函数直接返回。
 * - 熄灭逻辑会自动适配高/低电平有效。
 */
void LED_Off(LED_TypeDef led);

/**
 * @brief 翻转指定 LED 状态
 * @param led LED 编号
 * @note
 * - 若编号非法，函数直接返回。
 * - 翻转的是“当前物理引脚电平”，适用于快速状态切换。
 */
void LED_Toggle(LED_TypeDef led);

/**
 * @brief 点亮全部 LED
 * @note
 * - 按配置表顺序逐个点亮。
 * - 若 `LED_NUM` 与板级配置数量不一致，将以较小值为上限，避免越界。
 */
void LED_AllOn(void);

/**
 * @brief 熄灭全部 LED
 * @note
 * - 按配置表顺序逐个熄灭。
 * - 若 `LED_NUM` 与板级配置数量不一致，将以较小值为上限，避免越界。
 */
void LED_AllOff(void);

/**
 * @brief 阻塞式闪烁指定 LED
 * @param led LED 编号
 * @param period_ms 闪烁周期（单位：毫秒）
 * @param times 闪烁次数
 * @note
 * - 该接口内部使用 `HAL_Delay()`，为阻塞调用。
 * - 当 `period_ms==0` 或 `times==0` 或 `led` 非法时，不执行闪烁。
 * - 半周期为 `period_ms/2`，当 `period_ms` 为奇数时会存在 1ms 量化误差。
 */
void LED_Blink(LED_TypeDef led, uint32_t period_ms, uint8_t times);

#ifdef __cplusplus
}
#endif

#endif
