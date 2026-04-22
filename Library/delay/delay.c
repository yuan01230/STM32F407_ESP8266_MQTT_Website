//
// Created by 标语 on 26-3-8.
// 优化版：基于 DWT 的高精度延时
//

#include "delay.h"
#include <stdint.h>

// 全局变量，存储每微秒和每毫秒对应的时钟周期数
static uint32_t fac_us = 0;
static uint32_t fac_ms = 0;

/**
 * @brief 初始化延时函数
 * @note  配置 DWT 外设作为时基。
 *        DWT_CYCCNT 是一个 32 位计数器，每个时钟周期加 1。
 *        必须在系统时钟初始化完成后调用。
 */
void delay_init(void)
{
    // 1. 使能 DWT 外设 (TRCENA bit)
    // DEMCR: Debug Exception and Monitor Control Register
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // 2. 复位 DWT 计数器
    DWT->CYCCNT = 0;

    // 3. 使能 DWT 计数器 (CYCCNTENA bit)
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // 4. 计算延时倍乘数
    // SystemCoreClock 是 CMSIS 标准变量，需在 system_stm32f4xx.c 中正确更新
    if (SystemCoreClock == 0) {
        // 如果主频未获取到，默认使用 168MHz (常见 F407 频率)，防止除零或错误计算
        SystemCoreClock = 168000000;
    }

    fac_us = SystemCoreClock / 1000000U; // 每微秒需要的时钟周期数
    fac_ms = fac_us * 1000U;             // 每毫秒需要的时钟周期数
}

/**
 * @brief 微秒级延时 (高精度)
 * @param nus: 要延时的微秒数
 * @note 利用 DWT 计数器的自然回卷特性，无需担心 32 位溢出问题。
 *       公式：(Current - Start) < Target 即使在 Current 溢出后依然成立。
 */
void delay_us(uint32_t nus)
{
    uint32_t start_tick, delay_ticks;

    if (nus == 0) return; // 0 延时直接返回

    start_tick = DWT->CYCCNT;          // 记录开始时刻
    delay_ticks = nus * fac_us;        // 计算目标周期数

    // 等待直到经过足够的周期数
    // 注意：DWT->CYCCNT 是 32 位无符号数，减法运算自动处理回卷 (Overflow)
    while ((DWT->CYCCNT - start_tick) < delay_ticks)
    {
        // 可以在这里添加 __NOP() 以防止极端情况下编译器优化掉整个循环，
        // 但在大多数优化等级下，volatile 寄存器读取已足够阻止优化。
    }
}

/**
 * @brief 毫秒级延时
 * @param nms: 要延时的毫秒数
 * @note 内部调用 delay_us(1000) 累加实现，保证精度一致。
 */
void delay_ms(uint16_t nms)
{
    uint16_t i;
    for (i = 0; i < nms; i++)
    {
        delay_us(1000);
    }
}