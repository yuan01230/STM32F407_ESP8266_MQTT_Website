# STM32F407 仪表盘 + 阿里云 IoT 联动工程

这是一个由两部分组成的工程：

- `Project/`：STM32F407 主工程，负责传感器采集、LCD 显示、按键/蜂鸣器/LED 控制，以及 ESP8266 连接阿里云
- `Website/`：阿里云 IoT 可视化仪表盘网站，负责本地页面展示、模拟模式、OpenAPI 接入和设备控制

如果你刚接手这个项目，先看下面这份总览，再分别进入 `Project/README.md` 和 `Website/README.md`。

## 工程目录

- `Project/README.md`：主工程介绍、编译、下载和硬件连接
- `Website/README.md`：网站介绍、启动方式和阿里云接入

## 整个工程怎么用

1. 先在 `Project/` 中编译并下载 STM32 固件。
2. 按 `Project/README.md` 中的说明连接 DHT11、光敏 ADC、MPU6050、ESP8266、LCD 等硬件。
3. 在 `Website/` 中启动仪表盘网站，先用本地模拟模式验证页面，再按需配置阿里云 OpenAPI。
4. 固件和网站都准备好后，设备会通过 ESP8266 上报数据到阿里云，网站可以实时查看设备状态并下发控制指令。

## 使用入口

- 主工程源码：`Project/Core/`
- 主工程库文件：`Project/Library/`
- 网站源码：`Website/aliyun-iot-dashboard/`

