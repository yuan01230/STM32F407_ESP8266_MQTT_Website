# Aliyun IoT 驱动说明

## 1. 模块简介
`aliyun_iot` 模块用于管理 `ESP8266-01` 的 AT 通信流程，完成以下功能：

- 复位并初始化 ESP8266
- 连接 Wi-Fi 热点
- 连接阿里云 MQTT 服务器
- 周期性上报传感器属性
- 订阅属性设置主题
- 接收云端下发并控制 `LED0`、`LED1`

当前实现基于 `USART3 + ESP8266 AT 固件`，不在 STM32 侧自行实现 MQTT 协议栈。

## 2. 文件说明
- `aliyun_iot.h`：对外接口、数据结构声明
- `aliyun_iot.c`：AT 状态机、上报和下发处理实现

## 3. 硬件连接
- `PB10 -> USART3_TX -> ESP8266_RX`
- `PB11 -> USART3_RX -> ESP8266_TX`
- `PF6  -> ESP8266 EN`
- `PC0  -> ESP8266 RST`
- `3.3V -> VCC`
- `GND  -> GND`

注意：
- `ESP8266-01` 必须使用稳定的 `3.3V` 供电
- 串口波特率需与当前 AT 固件一致
- 本工程当前使用 `USART3`

## 4. 默认云端配置
模块配置文件 [aliyun_iot_config.h](/F:/CLion/Clion_Code/Wifi/Library/aliyun_iot/aliyun_iot_config.h:1) 当前集中定义了以下内容：

- Wi-Fi 名称和密码
- 阿里云 MQTT Host 和 Port
- 设备三元组推导出的 `ClientId / Username / Password`
- 属性上报主题
- 属性设置订阅主题
- 属性设置回复主题
- 周期上报间隔 `10s`

后续如果你要更换：

- 手机热点名称或密码
- 阿里云产品三元组对应的 MQTT 参数
- 主题路径
- 上报周期

优先只修改 `aliyun_iot_config.h`，不需要改 `aliyun_iot.c` 的驱动流程。

## 5. 对外接口
- `void AliyunIoT_Init(UART_HandleTypeDef *huart)`：初始化模块并启动接收中断
- `void AliyunIoT_Task(void)`：主循环轮询任务
- `void AliyunIoT_RequestPropertyPost(void)`：主动触发一次属性上报
- `void AliyunIoT_UpdateSensorData(const AliyunIoT_SensorData_t *data)`：更新待上报的传感器数据
- `void AliyunIoT_RxCpltCallback(UART_HandleTypeDef *huart)`：在 `HAL_UART_RxCpltCallback` 中转发
- `uint8_t AliyunIoT_IsReady(void)`：是否已完成联网和 MQTT 建链
- `const char *AliyunIoT_GetLastLine(void)`：最近一次 ESP8266 接收文本
- `uint8_t AliyunIoT_GetStep(void)`：当前 AT 状态机步骤
- `uint32_t AliyunIoT_GetOkCount(void)`：收到的 `OK` 次数

## 6. 数据结构
```c
typedef struct
{
  float temperature;
  float humidity;
  uint16_t light_adc;
  float light_voltage;
  float pitch;
  float roll;
  float yaw;
} AliyunIoT_SensorData_t;
```

## 7. 使用步骤
1. 完成 `MX_GPIO_Init()`、`MX_USART3_UART_Init()` 等底层初始化
2. 在外设初始化后调用 `AliyunIoT_Init(&huart3)`
3. 周期采集传感器数据
4. 调用 `AliyunIoT_UpdateSensorData(&sensor_data)` 更新上报缓存
5. 在主循环中持续调用 `AliyunIoT_Task()`
6. 在 `HAL_UART_RxCpltCallback()` 中调用 `AliyunIoT_RxCpltCallback(huart)`

## 8. main.c 集成示例
```c
AliyunIoT_SensorData_t sensor_data;

AliyunIoT_Init(&huart3);

while (1)
{
  sensor_data.temperature = g_dht_temperature_value;
  sensor_data.humidity = g_dht_humidity_value;
  sensor_data.light_adc = g_light_adc_value;
  sensor_data.light_voltage = g_light_voltage_value;
  sensor_data.pitch = g_pitch_value;
  sensor_data.roll = g_roll_value;
  sensor_data.yaw = g_yaw_value;

  AliyunIoT_UpdateSensorData(&sensor_data);
  AliyunIoT_Task();
}
```

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  AliyunIoT_RxCpltCallback(huart);
}
```

## 9. 当前属性模型
当前上报 JSON 中包含以下属性：

- `temperature`
- `Humidity`
- `LEDSwitch0`
- `LEDSwitch1`
- `Light_Out_Voltage`
- `Yaw`
- `Roll`
- `Pitch`
- `Light_ADC`

当前下发处理支持：

- `LEDSwitch0`
- `LEDSwitch1`

## 10. 调试说明
串口日志中常见关键字如下：

- `[ESP8266] TX:`：发送的 AT 指令
- `[ESP8266] RX:`：ESP8266 返回文本
- `+MQTTCONNECTED:`：MQTT 已连接
- `+MQTTPUB:OK`：属性上报成功
- `+MQTTSUBRECV:`：收到云端下发

如果连接失败，优先检查：
1. `ESP8266` 电源是否稳定
2. `USART3` 收发引脚和波特率是否正确
3. 热点是否为 `2.4G`
4. 三元组和 MQTT 参数是否匹配当前设备
5. 阿里云物模型标识符是否与代码一致
