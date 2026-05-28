# Delay 驱动说明

## 1. 模块简介
Delay 驱动同时提供两类能力：
- 阻塞延时：基于 DWT 周期计数器（高精度微秒/毫秒）
- 非阻塞延时：基于 TIM2 计数器（启动后轮询到期）

## 2. 文件说明
- `delay.h`：阻塞/非阻塞接口声明与类型定义。
- `delay.c`：DWT 与 TIM2 两套延时实现。
- `delay_example.c`：基础阻塞示例与非阻塞示例。

## 3. 依赖关系
- CMSIS：`core_cm4.h`（访问 DWT/CoreDebug 寄存器）
- STM32 HAL：`SystemCoreClock`、`TIM_HandleTypeDef`
- TIM2：由 CubeMX 生成并初始化

## 4. DWT 阻塞延时原理
1. Cortex-M 内核内置 `DWT->CYCCNT`，每个 CPU 时钟周期递增 1。
2. `delay_init()` 根据 `SystemCoreClock` 计算 `1us` 对应周期数 `s_fac_us`。
3. `delay_us(nus)` 计算目标周期数 `ticks = nus * s_fac_us`。
4. 忙等待直到 `(DWT->CYCCNT - start) >= ticks`。
5. 因为使用 32 位无符号差值比较，计数器回卷时仍能正确计时。

## 5. TIM2 非阻塞延时原理
1. 通过 `delay_nb_init()` 启动 TIM2 基本计数。
2. 调用 `delay_nb_start_us/ms()` 记录起始计数与目标超时。
3. 周期调用 `delay_nb_is_expired()` 检查是否到时。
4. 到时后函数返回 1 并自动清除 active 标志（单次定时语义）。

## 6. 接口说明（阻塞）
- `void delay_init(void)`：初始化 DWT 延时模块。
- `void delay_us(uint32_t nus)`：微秒级阻塞延时。
- `void delay_ms(uint16_t nms)`：毫秒级阻塞延时。

## 7. 接口说明（非阻塞）
- `HAL_StatusTypeDef delay_nb_init(void)`：初始化 TIM2 非阻塞底座。
- `void delay_nb_start_us(delay_nb_timer_t *timer, uint32_t timeout_us)`：启动微秒定时。
- `void delay_nb_start_ms(delay_nb_timer_t *timer, uint32_t timeout_ms)`：启动毫秒定时。
- `uint8_t delay_nb_is_expired(delay_nb_timer_t *timer)`：查询到期（到期自动失活）。
- `uint8_t delay_nb_is_active(const delay_nb_timer_t *timer)`：查询是否运行中。
- `void delay_nb_cancel(delay_nb_timer_t *timer)`：取消定时。

## 8. 使用说明
1. 先完成 `MX_TIM2_Init()`。
2. 阻塞延时场景：调用 `delay_init()` 后使用 `delay_us/ms`。
3. 非阻塞场景：定义 `delay_nb_timer_t` 变量并调用 `delay_nb_start_ms/us()`。
4. 在主循环中周期调用 `delay_nb_is_expired()` 做到时处理。

## 9. 注意事项
- 阻塞接口会占用 CPU，不适合长等待。
- 非阻塞接口适合状态机/轮询架构，不会阻塞主循环。
- 当前实现假设 TIM2 计数频率为 1MHz（1tick=1us）。若修改 TIM2 分频，请同步调整说明和换算。
- `delay_nb_start_ms()` 中 `timeout_ms * 1000` 在极大数值下可能溢出，超长超时建议分段处理。

## 10. 案例
- 阻塞示例：`DELAY_Example_Basic()`
- 非阻塞示例：`DELAY_Example_NonBlocking()`
