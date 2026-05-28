# Project - STM32F407 主工程

这个目录存放 STM32F407 主控工程，用于完成本地传感器仪表盘和阿里云 IoT 联动。

## 这个工程做什么

- 采集 `DHT11` 温湿度
- 采集光敏 ADC 数据并换算电压/亮度
- 读取 `MPU6050` 原始数据和 DMP 姿态角
- 驱动 `3.5` 寸 `TFT LCD` 仪表盘显示
- 通过 `ESP8266-01` 连接 Wi-Fi
- 通过 MQTT 接入阿里云 IoT
- 接收云端下发的 `LEDSwitch0 / LEDSwitch1` 控制指令

## 主要目录

- `Core/`：启动代码、主循环、外设初始化
- `Drivers/`：STM32 HAL 驱动
- `FATFS/`、`Middlewares/`：存储相关中间件
- `Library/`：业务驱动库和功能模块
- `cmake/`：CMake 工具链和子工程配置

## 如何使用

### 1. 打开工程

建议直接用 CLion 打开 `Project/` 目录。

### 2. 配置编译环境

工程使用 `CMake + STM32CubeMX` 管理，需要可用的 `arm-none-eabi-gcc` 工具链。

### 3. 编译

在 `Project/` 目录下执行：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

或者在 CLion 中直接选择 `Debug` 配置构建。

### 4. 下载到开发板

将生成的固件下载到 STM32F407 开发板后上电运行。

## 硬件连接

### 主控和显示

- MCU：`STM32F407`
- 显示：`3.5` 寸 `TFT LCD`

### 传感器

- `DHT11`：温湿度
- `Light Sensor`：光敏电阻 ADC 输入
- `MPU6050`：姿态和运动数据

### ESP8266-01

- `PB10 -> USART3_TX -> ESP8266_RX`
- `PB11 -> USART3_RX -> ESP8266_TX`
- `PF6  -> ESP8266 EN`
- `PC0  -> ESP8266 RST`
- `3.3V -> VCC`
- `GND  -> GND`

## 运行流程

1. 初始化 GPIO、USART、ADC、LCD、定时器和外设
2. 初始化 DHT11、光敏模块、MPU6050 和 DMP
3. 初始化 `AliyunIoT` 模块并复位 ESP8266
4. 在 LCD 上显示单页仪表盘
5. 每 `500ms` 刷新本地传感器数据
6. 每 `10s` 上报一次云端属性
7. 如果云端下发 `LEDSwitch0 / LEDSwitch1`，立即执行控制

## 配置说明

云端配置集中放在：

- `Project/Library/aliyun_iot/aliyun_iot_config.h`

这里通常包含：

- 热点/设备信息
- MQTT Host / Port
- ClientId / Username / Password
- 订阅和发布主题

如果后续你更换热点、设备或产品模型，优先改这个配置文件。

## 注意事项

- `DHT11` 的采样周期不建议低于 `1s`
- `ESP8266-01` 必须使用稳定的 `3.3V` 供电
- 光敏模块建议先按你的实际硬件重新校准阈值
- 串口日志对排查联网问题非常重要，建议保留 `printf`

