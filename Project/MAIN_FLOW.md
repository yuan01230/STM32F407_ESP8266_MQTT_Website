# 程序工作流程图

本文档描述 `Core/Src/main.c` 的主执行流程，重点覆盖启动初始化、周期传感器采集、LCD 显示刷新和阿里云 IoT 通信状态机。

```mermaid
flowchart TD
    A["程序上电 / 复位"] --> B["HAL_Init<br/>初始化 HAL、Flash、SysTick"]
    B --> C["SystemClock_Config<br/>配置系统时钟"]
    C --> D["CubeMX 外设初始化"]

    D --> D1["GPIO / DMA / FSMC"]
    D --> D2["SPI1 / ADC3 / SDIO"]
    D --> D3["USART1 / USART3 / TIM2 / FATFS"]

    D1 --> E["用户外设初始化"]
    D2 --> E
    D3 --> E

    E --> E1["LED / KEY / BEEP 初始化"]
    E1 --> E2["delay / 非阻塞定时器初始化"]
    E2 --> E3["TFTLCD 初始化"]
    E3 --> E4["显示开机界面"]

    E4 --> F["初始化传感器和通信模块"]
    F --> F1["拉高 ESP8266 相关引脚"]
    F1 --> F2["DHT11 初始化"]
    F2 --> F3["光敏传感器初始化"]
    F3 --> F4["AliyunIoT_Init<br/>USART3 连接 ESP8266"]
    F4 --> F5["MPU6050 初始化"]
    F5 --> F6{"MPU6050 初始化成功?"}

    F6 -- "是" --> F7["初始化 DMP 姿态解算"]
    F6 -- "否" --> F8["记录 MPU 初始化失败<br/>LCD 显示错误状态"]

    F7 --> F9{"DMP 初始化成功?"}
    F9 -- "是" --> G["显示主仪表盘界面"]
    F9 -- "否" --> F10["记录 DMP 初始化失败<br/>LCD 显示错误状态"]
    F8 --> G
    F10 --> G

    G --> H["立即执行一次 LCD_Test_Run<br/>刷新首屏数据"]
    H --> I["启动 500ms 非阻塞刷新定时器"]
    I --> J["进入 while(1) 主循环"]

    J --> K{"500ms 定时到?"}
    K -- "是" --> L["LCD_Test_Run<br/>采集传感器并刷新界面"]
    K -- "否" --> P["AliyunIoT_Task<br/>继续推进 ESP8266 / 云端状态机"]

    L --> M["读取 DHT11 温湿度"]
    M --> N["读取光敏 ADC<br/>换算电压和亮度等级"]
    N --> O{"MPU6050 可用?"}

    O -- "是" --> O1["读取加速度 / 陀螺仪 / 温度"]
    O -- "否" --> O2["LCD 显示 Raw failed"]

    O1 --> Q{"DMP 可用?"}
    O2 --> Q

    Q -- "是" --> Q1["读取 Pitch / Roll / Yaw"]
    Q -- "否" --> Q2["显示占位符或保留历史姿态角"]

    Q1 --> R["更新 AliyunIoT_SensorData"]
    Q2 --> R

    R --> S["刷新 LCD 底部状态栏<br/>ESP 最近响应 / 状态机步骤 / OK 次数"]
    S --> T["重新启动 500ms 定时器"]
    T --> P

    P --> J
```

## 主循环逻辑

1. 系统启动后先完成 HAL、系统时钟和 CubeMX 外设初始化。
2. 用户代码继续初始化 LED、按键、蜂鸣器、延时、LCD、DHT11、光敏传感器、ESP8266 和 MPU6050。
3. LCD 先显示启动状态，初始化完成后切换到传感器仪表盘页面。
4. 主循环中，传感器和 LCD 每 `500ms` 刷新一次。
5. `AliyunIoT_Task()` 每轮循环都会运行，用于持续推进 ESP8266 AT 指令和云端通信流程。

