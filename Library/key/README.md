# KEY 驱动说明

## 1. 模块简介
KEY 驱动采用“轮询 + 软件消抖 + 单次事件上报”机制。
驱动内部只负责把按键电平变化转换成统一事件，不直接控制 LED/BEEP 等业务外设，保证模块解耦。

## 2. 文件说明
- `key.h`：按键枚举、消抖宏、对外接口声明。
- `key.c`：按键状态机实现（消抖、上报锁、释放复位）。
- `key_example.c`：主循环轮询示例。

## 3. 依赖关系
- `gpio_device` 通用层：统一读取逻辑电平。
- `board_drv_config.h`：按键端口/引脚/有效电平配置。
- STM32 HAL Tick：用于毫秒级消抖计时。

## 4. 状态机说明
每个按键维护 3 个运行时状态：
- `stable_pressed`：当前稳定状态（按下/释放）
- `reported`：当前按下是否已上报过
- `edge_tick`：最近变化计时基准

处理流程：
1. 采样值与稳定值不同：进入消抖观察
2. 变化持续超过 `KEY_DEFAULT_DEBOUNCE_MS`：确认状态切换
3. 确认为按下且未上报：上报一次事件并锁定
4. 确认为释放：清除锁定，允许下一次按下再次上报

## 5. 接口说明
- `void Key_Init(void)`
  - 初始化运行时状态机
  - 建议系统初始化后调用一次
- `KeyName_t Key_Scan(void)`
  - 每次调用最多返回一个按键事件
  - 无事件返回 `KEY_NONE`

## 6. 使用说明
1. 完成 GPIO 初始化（`MX_GPIO_Init()`）。
2. 调用一次 `Key_Init()`。
3. 在主循环中每 5~10ms 调用 `Key_Scan()`。
4. 根据返回值在业务层执行动作（如切换 LED、控制 BEEP）。

## 7. 注意事项
- 若主循环长时间阻塞，按键扫描频率下降，会影响响应速度。
- 若硬件抖动较严重，可适当增大 `KEY_DEFAULT_DEBOUNCE_MS`。
- 扩展按键时，必须同步更新：
  - `BOARD_KEY_COUNT`
  - `BOARD_KEY_CONFIG_TABLE`
  - `KeyName_t` 枚举顺序

## 8. 案例
参考 `key_example.c` 中 `KEY_Example_Polling()`。
