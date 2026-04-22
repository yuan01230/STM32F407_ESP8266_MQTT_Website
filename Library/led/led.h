//
// Created by 标语 on 25-12-30.
//

#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

    /**
     * @brief LED 编号枚举
     *
     * 如果后续增加 LED，只需要在这里加
     */
    typedef enum
    {
        LED0 = 0,
        LED1,
        LED_NUM
    } LED_TypeDef;

    /* ===================== 接口函数 ===================== */

    /**
     * @brief  LED 驱动初始化
     * @note   目前 GPIO 已由 CubeMX 初始化，此函数用于逻辑层初始化
     */
    void LED_Init(void);

    /**
     * @brief  打开指定 LED
     * @param  led: LED 编号
     */
    void LED_On(LED_TypeDef led);

    /**
     * @brief  关闭指定 LED
     * @param  led: LED 编号
     */
    void LED_Off(LED_TypeDef led);

    /**
     * @brief  翻转指定 LED 状态
     * @param  led: LED 编号
     */
    void LED_Toggle(LED_TypeDef led);

    /**
     * @brief  打开所有 LED
     */
    void LED_AllOn(void);

    /**
     * @brief  关闭所有 LED
     */
    void LED_AllOff(void);

    /**
     * @brief  LED 闪烁（阻塞式）
     * @param  led:   LED 编号
     * @param  period_ms: 闪烁周期（ms）
     * @param  times: 闪烁次数
     */
    void LED_Blink(LED_TypeDef led, uint32_t period_ms, uint8_t times);

#ifdef __cplusplus
}
#endif


#endif //LED_H
