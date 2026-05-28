#include "delay.h"

/* TIM2 句柄由 CubeMX 生成于 main.c */
extern TIM_HandleTypeDef htim2;

/* 每 1 微秒对应的 CPU 时钟周期数（DWT 阻塞延时使用） */
static uint32_t s_fac_us = 0U;
/* DWT 初始化状态标志 */
static uint8_t s_delay_inited = 0U;
/* TIM2 非阻塞延时底座初始化状态 */
static uint8_t s_delay_nb_inited = 0U;


/**
 * @brief 初始化 DWT 阻塞延时模块
 * @note
 * - 初始化后可使用 delay_us()/delay_ms() 进行高精度阻塞延时
 * - 该函数与 TIM2 非阻塞延时相互独立
 */
void delay_init(void)
{
    /*
     * DWT 延时原理：
     * 1) Cortex-M 内核内置 DWT->CYCCNT 周期计数器，每个 CPU 时钟周期 +1。
     * 2) 将“目标延时时间(微秒)”转换成“目标周期数 ticks”。
     * 3) 循环等待直到 (当前计数 - 起始计数) >= ticks。
     * 4) 利用无符号差值比较可自动处理 32 位计数器回卷。
     */

    /* 使能 DWT 跟踪功能（TRCENA） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 清零周期计数器，避免使用历史计数值 */
    DWT->CYCCNT = 0U;

    /* 开启周期计数 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 主频防御性处理：异常为 0 时回退默认值 */
    if (SystemCoreClock == 0U) {
        SystemCoreClock = 168000000U;
    }

    /* 计算每微秒对应周期数，用于后续时间换算 */
    s_fac_us = SystemCoreClock / 1000000U;

    /* 极端保护：避免错误时钟配置导致 0 */
    if (s_fac_us == 0U) {
        s_fac_us = 168U;
    }

    s_delay_inited = DELAY_DWT_INIT_DONE;
}

/**
 * @brief 微秒级阻塞延时
 * @param nus 需要延时的微秒数
 * @note
 * - 该函数为忙等待实现，延时期间 CPU 会一直停留在当前函数内
 * - 适合 SPI/LCD/传感器等对微秒时序有要求的短延时场景
 * - 若 `nus` 为 0，函数直接返回
 */
void delay_us(uint32_t nus)
{
    if (nus == 0U) {
        return;
    }

    if (s_delay_inited != DELAY_DWT_INIT_DONE) {
        delay_init();
    }

    const uint32_t start = DWT->CYCCNT;
    const uint32_t ticks = nus * s_fac_us;

    while ((DWT->CYCCNT - start) < ticks) {
    }
}

/**
 * @brief 毫秒级阻塞延时
 * @param nms 需要延时的毫秒数
 * @note
 * - 内部通过循环调用 `delay_us(1000)` 实现
 * - 该函数同样属于阻塞调用，不适合长时间等待任务
 * - 适合初始化阶段、简单外设驱动时序等场景
 */
void delay_ms(uint16_t nms)
{
    for (uint16_t i = 0U; i < nms; ++i) {
        delay_us(1000U);
    }
}


/**
 * @brief 读取 TIM2 当前计数值（单位：us）
 * @return uint32_t 32 位计数值
 * @note
 * - 依赖当前工程 TIM2 配置为 1MHz 计数频率（1 计数 = 1us）
 * - 采用 32 位计数可天然支持回卷差值计算
 */
static uint32_t delay_nb_get_tick_us(void)
{
    return __HAL_TIM_GET_COUNTER(&htim2);
}


/**
 * @brief 初始化 TIM2 非阻塞延时底座
 * @return HAL_StatusTypeDef HAL_OK 表示初始化成功
 * @note
 * - 依赖 MX_TIM2_Init() 已执行
 * - 内部仅在首次调用时启动 TIM2，重复调用会直接返回 HAL_OK
 */
HAL_StatusTypeDef delay_nb_init(void)
{
    if (s_delay_nb_inited == DELAY_NB_TIMER_ACTIVE) {
        return HAL_OK;
    }

    /* 启动 TIM2 基本计数，为非阻塞超时判断提供统一时间基准 */
    HAL_StatusTypeDef ret = HAL_TIM_Base_Start(&htim2);
    if (ret == HAL_OK) {
        s_delay_nb_inited = DELAY_NB_TIMER_ACTIVE;
    }
    return ret;
}

/**
 * @brief 启动一个“微秒”为单位的非阻塞定时器
 * @param timer 定时器实例指针
 * @param timeout_us 超时时长（单位：us）
 * @note
 * - 调用后函数立即返回，不阻塞 CPU
 * - 到期判断请在主循环中调用 delay_nb_is_expired()
 */
void delay_nb_start_us(delay_nb_timer_t *timer, uint32_t timeout_us)
{
    if (timer == NULL) {
        return;
    }

    if (s_delay_nb_inited != DELAY_NB_TIMER_ACTIVE) {
        if (delay_nb_init() != HAL_OK) {
            timer->active = 0U;
            return;
        }
    }

    /* 记录启动时刻与目标超时长度，置为运行中 */
    timer->start_tick_us = delay_nb_get_tick_us();
    timer->timeout_us = timeout_us;
    timer->active = DELAY_NB_TIMER_ACTIVE;
}

/**
 * @brief 启动一个“毫秒”为单位的非阻塞定时器
 * @param timer 定时器实例指针
 * @param timeout_ms 超时时长（单位：ms）
 * @note 内部会换算为微秒并调用 delay_nb_start_us()
 */
void delay_nb_start_ms(delay_nb_timer_t *timer, uint32_t timeout_ms)
{
    delay_nb_start_us(timer, timeout_ms * 1000U);
}

/**
 * @brief 查询定时器是否到期
 * @param timer 定时器实例指针
 * @return uint8_t 1=到期，0=未到期或未激活
 * @note
 * - 本函数采用“单次触发”语义：返回 1 时会自动清除 active 标志
 * - 如需周期定时，请在到期后再次调用 delay_nb_start_xx() 重新启动
 */
uint8_t delay_nb_is_expired(delay_nb_timer_t *timer)
{
    if (timer == NULL || timer->active != DELAY_NB_TIMER_ACTIVE) {
        return 0U;
    }

    const uint32_t now = delay_nb_get_tick_us();

    /*
     * 回卷安全比较：
     * now/start 为 32 位无符号数，(now - start) 在回卷后仍为正确经过量。
     */
    if ((now - timer->start_tick_us) >= timer->timeout_us) {
        timer->active = 0U;
        return 1U;
    }

    return 0U;
}

/**
 * @brief 查询定时器是否仍在运行
 * @param timer 定时器实例指针
 * @return uint8_t 1=运行中，0=未运行
 */
uint8_t delay_nb_is_active(const delay_nb_timer_t *timer)
{
    if (timer == NULL) {
        return 0U;
    }

    return (timer->active == DELAY_NB_TIMER_ACTIVE) ? 1U : 0U;
}

/**
 * @brief 取消一个非阻塞定时器
 * @param timer 定时器实例指针
 * @note 取消后 delay_nb_is_expired() 将返回未到期（0）
 */
void delay_nb_cancel(delay_nb_timer_t *timer)
{
    if (timer == NULL) {
        return;
    }

    timer->active = 0U;
}
