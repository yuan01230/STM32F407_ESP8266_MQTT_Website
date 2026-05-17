# BEEP 驱动说明

## 1. 模块简介
蜂鸣器驱动基于通用 GPIO 抽象层实现，电平有效性由板级配置决定。

## 2. 文件说明
- `beep.h`：对外接口与状态枚举。
- `beep.c`：驱动实现。
- `beep_example.c`：蜂鸣案例。

## 3. 依赖关系
- `gpio_device` 通用层
- `board_drv_config.h` 板级配置
- STM32 HAL

## 4. 接口说明
- `void BEEP_Init(void)`：初始化（默认关闭）。
- `void BEEP_On(void)`：打开蜂鸣器。
- `void BEEP_Off(void)`：关闭蜂鸣器。
- `void BEEP_Toggle(void)`：翻转状态。
- `void BEEP_Set(BEEP_Status_t status)`：按状态设置。

## 5. 使用说明
1. 确保 GPIO 已初始化。
2. 调用 `BEEP_Init()`。
3. 通过 `BEEP_On/Off/Set` 控制蜂鸣器。

## 6. 案例
参考 `beep_example.c` 的 `BEEP_Example_Beep100ms()`。
