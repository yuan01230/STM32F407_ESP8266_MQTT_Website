# Light Sensor 驱动说明

## 1. 模块简介
光敏传感器驱动基于 `ADC3` 采样实现，提供以下能力：

- 读取单次原始 ADC 值
- 多次采样平均
- ADC 转电压
- 按 ADC 区间映射光照等级

当前硬件表现为“光越强，输出电压越低，ADC 值越小”。

## 2. 文件说明
- `light_sensor.h`：对外接口、枚举和说明
- `light_sensor.c`：驱动实现
- `light_sensor_example.c`：最小使用示例

## 3. 依赖关系
- STM32 HAL
- `MX_ADC3_Init()`

## 4. 硬件连接
- 传感器模拟输出接 `PF7 (ADC3_IN5)`
- `VCC` 接 `3.3V`
- `GND` 接地

## 5. ADC 配置建议
当前工程使用：

- `ADC3`
- `12-bit` 分辨率
- 单次转换模式
- `ADC_CHANNEL_5`
- `ADC_SAMPLETIME_84CYCLES`

说明：

- 光敏模块输出阻抗通常比按键、电位器更高，采样时间过短时读数容易偏慢或不稳定。
- 因此这里不建议继续使用 `3 cycles`，改为 `84 cycles` 更稳妥。

## 6. 接口说明
- `void LightSensor_Init(void)`：统一初始化入口
- `uint16_t LightSensor_ReadADC(void)`：读取一次原始 ADC
- `uint16_t LightSensor_ReadADCAverage(uint8_t times)`：多次采样求平均
- `float LightSensor_ConvertToVoltage(uint16_t adc_value)`：将 ADC 转为电压
- `LightLevel_t LightSensor_GetLevelByADC(uint16_t adc_value)`：根据 ADC 判断光照等级
- `const char *LightSensor_LevelToString(LightLevel_t level)`：将等级转为字符串

## 7. 默认采样策略
当前默认配置：

- 默认平均次数：`3`
- 每次采样间隔：`1 ms`

这样做的目的：

- 比原来的 `10 次 + 5ms 间隔` 响应更快
- 仍保留少量平均，避免数值过于抖动

如果你后续更关注“快速响应”，可以继续降到 `1` 次平均；如果更关注“数值稳定”，可以提高到 `5` 次。

## 8. 当前阈值
根据你实测的大致范围：

- 手电照射时约 `2.3V`
- 不照射时约 `3.3V`

折算 ADC 后，当前驱动默认阈值为：

- `< 2600`：`VERY_BRIGHT`
- `< 2900`：`BRIGHT`
- `< 3200`：`NORMAL`
- `< 3600`：`DIM`
- `>= 3600`：`DARK`

这组阈值明显比旧版更贴近你的实际量程，亮暗变化会更容易分级显示出来。

## 9. 使用说明
1. 先执行 `MX_ADC3_Init()`
2. 调用 `LightSensor_Init()`
3. 优先先读取一次平均 ADC
4. 再基于同一份 ADC 数据换算电压和等级

推荐写法：

```c
uint16_t adc = LightSensor_ReadADCAverage(3);
float voltage = LightSensor_ConvertToVoltage(adc);
LightLevel_t level = LightSensor_GetLevelByADC(adc);
const char *level_text = LightSensor_LevelToString(level);
```

## 10. 调试建议
如果后续你发现分级仍不理想，优先检查这三项：

1. 实测最亮和最暗时的 ADC 范围
2. 采样平均次数是否过大
3. ADC 采样时间是否过短

这个模块本质上和硬件分压、电源、环境光强有关，阈值最终应以实测数据为准。
