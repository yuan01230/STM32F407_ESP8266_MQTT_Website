#include "delay.h"

/**
 * @brief Delay 驱动基础调用示例
 * @note
 * - 演示 DWT 初始化、毫秒延时、微秒延时组合调用
 * - delay_us/delay_ms 为阻塞式接口
 */
void DELAY_Example_Basic(void)
{
    /* 初始化 DWT 延时模块（建议系统启动后尽早调用一次） */
    delay_init();

    /* 阻塞延时 1ms */
    delay_ms(1);

    /* 再阻塞延时 100us */
    delay_us(100);
}

/**
 * @brief 基于 TIM2 的非阻塞延时示例
 * @note
 * - 本示例展示“启动后轮询到期”的单次定时模式
 * - 使用前需确保 `MX_TIM2_Init()` 已经执行
 */
void DELAY_Example_NonBlocking(void)
{
    static delay_nb_timer_t s_timer;

    /* 启动一个 500ms 非阻塞定时器 */
    delay_nb_start_ms(&s_timer, 500U);

    /*
     * 业务主循环中轮询：
     * if (delay_nb_is_expired(&s_timer)) {
     *     // 到时执行任务
     * }
     */
    if (delay_nb_is_expired(&s_timer)) {
        /* 这里放置“到时后执行”的业务逻辑 */
    }
}
