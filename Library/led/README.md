# LED 驱动说明

## 1. 模块简介
LED 驱动基于通用 GPIO 抽象层实现，支持多 LED 配置表静态注册。

## 2. 文件说明
- `led.h`：对外接口、类型声明。
- `led.c`：驱动实现。
- `led_example.c`：最小使用案例。

## 3. 依赖关系
- `gpio_device` 通用层
- `board_drv_config.h` 板级配置
- STM32 HAL

## 4. 接口说明
- `void LED_Init(void)`：初始化（默认全灭）。
- `void LED_On(LED_TypeDef led)`：点亮指定 LED。
- `void LED_Off(LED_TypeDef led)`：熄灭指定 LED。
- `void LED_Toggle(LED_TypeDef led)`：翻转指定 LED。
- `void LED_AllOn(void)`：全部点亮。
- `void LED_AllOff(void)`：全部熄灭。
- `void LED_Blink(LED_TypeDef led, uint32_t period_ms, uint8_t times)`：阻塞闪烁。

## 5. 使用说明
1. 确保先执行 `MX_GPIO_Init()`。
2. 调用 `LED_Init()`。
3. 调用 `LED_On/Off/Toggle` 等接口。

## 6. 案例
参考 `led_example.c` 的 `LED_Example_RunOnce()`。
