# DHT11 驱动说明

## 1. 模块简介
DHT11 驱动用于读取单总线温湿度传感器数据，输出湿度和温度的整数/小数部分。

## 2. 文件说明
- `dht11.h`：对外接口、数据结构声明。
- `dht11.c`：驱动实现。
- `dht11_example.c`：最小使用案例。

## 3. 依赖关系
- `delay` 延时模块
- STM32 HAL
- `MX_GPIO_Init()`

## 4. 硬件连接
- `VCC`：3.3V 或 5V
- `GND`：GND
- `DATA`：当前工程配置为 `PG9`
- `DATA` 需要外接 `4.7k~10k` 上拉电阻

## 5. 接口说明
- `void DHT11_Init(void)`：初始化驱动，最终保持输入上拉状态。
- `uint8_t DHT11_ReadData(DHT11_Data_t *data)`：读取一次温湿度数据，`0` 表示成功。

## 6. 使用说明
1. 确保先执行 `MX_GPIO_Init()`。
2. 确保先执行 `delay_init()`。
3. 调用 `DHT11_Init()`。
4. 以不小于 1 秒的周期调用 `DHT11_ReadData()`。

## 7. 案例
参考 `dht11_example.c` 的 `DHT11_Example_RunOnce()`。
