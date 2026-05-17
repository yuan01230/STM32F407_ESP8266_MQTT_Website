#ifndef DELAY_H
#define DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "core_cm4.h"

/**
 * @brief DWT 延时模块初始化完成标志值
 */
#define DELAY_DWT_INIT_DONE 1U

/**
 * @brief 非阻塞定时器活动标志值
 */
#define DELAY_NB_TIMER_ACTIVE 1U

/**
 * @brief 非阻塞定时器实例
 * @note
 * - 可在业务层定义多个实例，实现多个并行超时控制
 * - 时间基准由 TIM2 计数器提供（默认按 1us 递增）
 */
typedef struct {
    uint32_t start_tick_us; /**< 启动时刻（TIM2 计数值，单位 us） */
    uint32_t timeout_us;    /**< 超时时长（单位 us） */
    uint8_t active;         /**< 是否激活：1=运行中，0=未运行 */
} delay_nb_timer_t;

/**
 * @brief 初始化 DWT 高精度延时模块
 * @note
 * - 应在系统时钟配置完成后调用
 * - 若未显式调用，`delay_us()` 内部会自动惰性初始化
 */
void delay_init(void);

/**
 * @brief 微秒级阻塞延时
 * @param nus 延时微秒数
 * @note
 * - 该接口为阻塞调用
 * - 适合短时精确延时，不建议用于长时间等待
 */
void delay_us(uint32_t nus);

/**
 * @brief 毫秒级阻塞延时
 * @param nms 延时毫秒数
 * @note
 * - 该接口内部通过 `delay_us(1000)` 循环实现
 * - 属于阻塞调用
 */
void delay_ms(uint16_t nms);

/**
 * @brief 初始化 TIM2 非阻塞延时底座
 * @return HAL_StatusTypeDef HAL_OK 表示可用
 * @note
 * - 依赖 `MX_TIM2_Init()` 已先执行
 * - 内部会启动 TIM2 基本计数
 */
HAL_StatusTypeDef delay_nb_init(void);

/**
 * @brief 启动一个非阻塞微秒定时器
 * @param timer 定时器实例指针
 * @param timeout_us 超时时长（us）
 */
void delay_nb_start_us(delay_nb_timer_t *timer, uint32_t timeout_us);

/**
 * @brief 启动一个非阻塞毫秒定时器
 * @param timer 定时器实例指针
 * @param timeout_ms 超时时长（ms）
 */
void delay_nb_start_ms(delay_nb_timer_t *timer, uint32_t timeout_ms);

/**
 * @brief 查询非阻塞定时器是否到期
 * @param timer 定时器实例指针
 * @return uint8_t 1=已到期，0=未到期或未激活
 * @note
 * - 到期后会自动清除 active 标志（单次定时语义）
 */
uint8_t delay_nb_is_expired(delay_nb_timer_t *timer);

/**
 * @brief 查询非阻塞定时器是否处于运行状态
 * @param timer 定时器实例指针
 * @return uint8_t 1=运行中，0=未运行
 */
uint8_t delay_nb_is_active(const delay_nb_timer_t *timer);

/**
 * @brief 取消非阻塞定时器
 * @param timer 定时器实例指针
 */
void delay_nb_cancel(delay_nb_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif
