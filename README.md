# STM32F407 SD 文件浏览与音乐播放项目

## 项目简介

本工程基于普中麒麟 STM32F407 开发板，实现了一个以 SD 卡文件浏览为入口的综合示例程序。

当前主要功能：

- SDIO + FATFS 挂载 SD 卡。
- TFT LCD 文件浏览。
- 文本/普通文件查看。
- BMP/JPG 图片预览。
- 外部 Flash 字库存储和中文显示。
- WAV 音乐播放。
- MP3 音乐播放。
- 播放过程中按键调节音量。

## 当前音乐播放状态

音频链路已经按阶段完成验证：

1. WM8978 软件 IIC 控制验证。
2. I2S2 + DMA 方波 tone 测试。
3. WAV PCM 文件播放。
4. MP3 文件读取和帧头探测。
5. MP3 软件解码。
6. MP3 I2S DMA 双缓冲播放。

已验证现象：

- WM8978 可以稳定 ACK，地址为 `0x1A`。
- WAV 文件可以正常播放。
- MP3 文件可以正常播放。
- MP3 播放中的断续/快进问题已经修复。

## 硬件配置

| 模块 | 配置 |
| --- | --- |
| MCU | STM32F407 |
| 显示 | TFT LCD 并口屏 |
| 文件系统 | SDIO + FATFS |
| 字库存储 | 外部 Flash |
| 音频芯片 | WM8978 |
| 音频输出 | I2S2 |
| 音频 DMA | SPI2_TX / DMA1_Stream4 |

音频相关引脚：

| STM32 引脚 | 功能 |
| --- | --- |
| `PB12` | `I2S2_WS` |
| `PB13` | `I2S2_CK` |
| `PC3` | `I2S2_SD` |
| `PC6` | `I2S2_MCK` |

## 按键操作

| 页面 | `KEY0` | `KEY1` | `KEY2` | `KEY_UP` |
| --- | --- | --- | --- | --- |
| 主页面 | 进入文件浏览 | 切换文字颜色 | 创建测试文件 | 重新导入字体 |
| 文件列表 | 返回主页面 | 选择下一项 | 进入目录/打开文件 | 选择上一项 |
| 文件查看 | 返回列表 | 无 | 返回列表 | 无 |
| WAV/MP3 播放中 | 停止当前播放 | 降低音量 | 暂停/继续 | 增加音量 |

## 音乐播放使用方式

1. 将音乐文件放到 SD 卡，例如 `0:/Music` 目录。
2. 上电后等待 SD 卡和字体初始化完成。
3. 按 `KEY0` 进入文件浏览。
4. 选择 `Music` 目录并按 `KEY2` 进入。
5. 选择 `.wav` 或 `.mp3` 文件并按 `KEY2` 播放。
6. 播放过程中：
   - `KEY0` 停止当前播放。
   - `KEY1` 降低音量。
   - `KEY2` 暂停/继续。
   - `KEY_UP` 增加音量。

## 支持格式

### WAV

当前支持：

- PCM
- 44.1kHz
- 16bit
- 双声道

不支持的 WAV 会在串口输出 `UNSUPPORTED` 或格式错误信息。

### MP3

当前主要验证：

- Layer III MP3
- 44.1kHz
- 双声道

MP3 使用 `minimp3` 软件解码，再通过 I2S2 DMA 输出到 WM8978。

## 核心模块

| 模块 | 路径 | 作用 |
| --- | --- | --- |
| UI | `Library/ui/` | 页面切换、目录浏览、文件打开 |
| 音频播放 | `Library/audio/` | WAV/MP3 播放和音频测试 |
| WM8978 | `Library/wm8978/` | 音频芯片寄存器配置 |
| 软件 IIC | `Library/SoftwareI2C/` | WM8978 控制总线 |
| LCD | `Library/tftlcd/` | 屏幕显示 |
| 字库存储 | `Library/font_storage/` | 外部 Flash 字库管理 |
| FATFS | `FATFS/`、`Middlewares/Third_Party/FatFs/` | SD 卡文件系统 |

## 音频实现文档

| 文档 | 说明 |
| --- | --- |
| [Library/audio/README.md](Library/audio/README.md) | WAV/MP3 播放模块、调用链和实现思路 |
| [Library/wm8978/README.md](Library/wm8978/README.md) | WM8978 初始化、寄存器写入和播放路径 |
| [docs/superpowers/plans/2026-05-07-mp3-module-verification.md](docs/superpowers/plans/2026-05-07-mp3-module-verification.md) | MP3/WAV 验证过程和阶段记录 |

## 构建说明

本工程使用 CLion + STM32CubeCLT/CMake 构建。

常用命令：

```bash
cmake --preset Debug
cmake --build --target 1_LED --preset Debug
```

当前开发约定：

- 由开发者在 CLion 中编译和烧录。
- 代码生成仍由 CubeMX 负责。
- 用户代码应尽量放在 `USER CODE` 区域，避免重新生成代码时丢失。

## 当前限制和后续计划

当前未完成的计划项主要是播放器控制完善：

- 上一首/下一首。
- LCD 显示当前播放状态、文件名和音量。
- 支持更多采样率的 WAV/MP3。
- 将测试型 `mp3_decode_test` 重构为正式 `mp3_player` 模块。
