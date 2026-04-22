#ifndef __DELAY_H
#define __DELAY_H

#include "main.h"  // 确保包含 STM32 HAL 定义，获取 SystemCoreClock
#include "core_cm4.h" // 或者 core_cm7.h，取决于你的芯片内核，通常 main.h 会间接包含

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief 初始化延时函数 (基于 DWT)
     * @note 必须在 main() 函数的最开始调用，且在调用 delay_us/ms 之前调用。
     */
    void delay_init(void);

    /**
     * @brief 微秒级延时
     * @param nus: 延时的微秒数 (最大约 4294 秒 / fac_us，对于 168MHz 约为 25 秒)
     */
    void delay_us(uint32_t nus);

    /**
     * @brief 毫秒级延时
     * @param nms: 延时的毫秒数 (最大 65535 ms)
     */
    void delay_ms(uint16_t nms);

#ifdef __cplusplus
}
#endif

#endif /* __DELAY_H */