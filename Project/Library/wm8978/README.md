# WM8978 音频芯片驱动说明

## 模块目标

`Library/wm8978` 目录负责控制板载 WM8978 音频编解码芯片，使 STM32F407 通过 I2S2 输出的 PCM 数据能够从耳机或喇叭接口播放出来。

当前驱动完成：

- 软件 IIC 初始化。
- WM8978 地址探测。
- WM8978 9bit 寄存器写入。
- 软件复位。
- 电源管理配置。
- I2S Philips、16bit 数据格式配置。
- DAC、耳机输出、喇叭输出通路配置。
- 耳机音量和喇叭音量设置。

## 硬件关系

WM8978 和 STM32F407 之间有两条主要通路：

| 通路 | 作用 |
| --- | --- |
| 软件 IIC | STM32 写 WM8978 控制寄存器 |
| I2S2 | STM32 向 WM8978 发送 PCM 音频数据 |

当前工程中，WM8978 的控制地址实测为 7bit 地址 `0x1A`。

I2S2 使用的主要信号：

- `PB12` -> `I2S2_WS`
- `PB13` -> `I2S2_CK`
- `PC3` -> `I2S2_SD`
- `PC6` -> `I2S2_MCK`

## 寄存器写入格式

WM8978 的寄存器数据是 9bit，软件 IIC 写入时分两字节发送：

```text
第 1 字节：reg[6:0] << 1 | data[8]
第 2 字节：data[7:0]
```

对应实现：

```c
SoftwareI2C_SendByte((reg << 1) | ((value >> 8) & 0x01));
SoftwareI2C_SendByte(value & 0xFF);
```

因为 WM8978 寄存器主要是只写寄存器，驱动使用 `wm8978_reg_cache[]` 保存最近写入值，便于后续按位修改。

## 初始化思路

`WM8978_Init()` 是主要入口，执行顺序如下：

1. 初始化软件 IIC。
2. 设置 IIC 延时。
3. 释放总线并延时。
4. 探测 `0x1A` 地址是否 ACK。
5. 写 R0 执行软件复位。
6. 配置电源管理寄存器。
7. 配置 DAC、输出混音和耳机/喇叭输出。
8. 设置默认音量。
9. 配置 I2S 格式。
10. 调用 `WM8978_EnablePlayback()` 打开播放路径。

## 函数调用思路

### 播放前初始化

WAV、MP3 和 tone 测试在播放前都会调用：

```c
WM8978_Init();
```

这样可以确保每次播放前 WM8978 都回到已知配置，避免上一次测试残留寄存器状态影响本次播放。

### 音量设置

播放模块通过下面两个接口同步设置输出音量：

```c
WM8978_SetHPVolume(left, right);
WM8978_SetSPKVolume(volume);
```

其中：

- 耳机音量使用寄存器 R52/R53。
- 喇叭音量使用寄存器 R54/R55。
- 右声道寄存器写入 update 位，使左右声道音量同步更新。

### I2S 格式配置

当前使用：

```c
WM8978_ConfigI2S(2U, 0U);
```

含义：

- `format = 2`：I2S Philips 标准。
- `length = 0`：16bit 数据长度。

这与 CubeMX 中 SPI2/I2S2 的配置保持一致。

## 错误码约定

`WM8978_WriteReg()` 返回值：

- `0`：写入成功。
- `1`：寄存器编号非法。
- `2`：器件地址阶段无 ACK。
- `3`：寄存器地址/高位数据阶段无 ACK。
- `4`：低 8bit 数据阶段无 ACK。

`WM8978_Init()` 如果地址探测失败并且没有开启忽略 ACK，会返回 `2`。

## 与 audio 模块的关系

`audio` 模块只负责产生或读取 PCM 数据，真正的模拟输出路径由 WM8978 驱动配置。

典型调用关系：

```text
WAV/MP3/AudioTest
  -> WM8978_Init()
  -> WM8978_SetHPVolume()
  -> WM8978_SetSPKVolume()
  -> HAL_I2S_Transmit() 或 HAL_I2S_Transmit_DMA()
```

## 调试建议

如果没有声音，优先按下面顺序排查：

1. 串口是否打印 `[WM8978] addr 0x1A ACK`。
2. I2S DMA 的 NDTR 是否变化。
3. WAV 是否可以正常播放。
4. MP3 的 `underrun` 是否为 0。
5. 耳机是否插在正确输出口。

如果 WM8978 没有 ACK，先排查软件 IIC 的 SDA/SCL 引脚、电平释放、ACK 阶段 SDA 输入方向。
