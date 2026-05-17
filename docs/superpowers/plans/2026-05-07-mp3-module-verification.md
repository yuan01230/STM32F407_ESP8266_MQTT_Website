# MP3/WAV 音乐播放功能实现说明

## 目标

在普中麒麟 STM32F407 开发板上，基于当前工程已有的 SDIO、FATFS、LCD 文件浏览、按键扫描和 USART1 调试输出，实现音乐文件播放验证。

当前已经完成的功能：

- SD 卡文件浏览保持原有功能。
- 选择 `.wav` 文件后，按 `KEY2` 播放 WAV。
- 选择 `.mp3` 文件后，按 `KEY2` 播放 MP3。
- 播放过程中：
  - `KEY0` 停止当前播放。
  - `KEY1` 降低音量。
  - `KEY2` 暂停/继续。
  - `KEY_UP` 增加音量。
- 图片、文档和普通文件打开逻辑保持原有行为。

## 阶段完成情况

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| Stage 1 | WM8978 + I2S2 + DMA 方波 tone 验证 | 已完成 |
| Stage 2 | WAV PCM 播放验证 | 已完成 |
| Stage 3 | MP3 文件读取和帧头探测 | 已完成 |
| Stage 4 | MP3 软件解码验证 | 已完成 |
| Stage 5 | MP3 解码接入 I2S DMA 播放 | 已完成 |
| Stage 6 | 播放器控制完善 | 部分完成 |

下一步继续计划时，继续 Stage 6 的剩余控制功能：上一首/下一首、LCD 播放状态。

## 硬件和 CubeMX 配置基础

音频输出使用板载 WM8978 音频编解码芯片，主控通过 I2S2 输出 PCM 数据。

当前工程使用的音频相关连接：

- `PB12` -> `I2S2_WS`
- `PB13` -> `I2S2_CK`
- `PC3` -> `I2S2_SD`
- `PC6` -> `I2S2_MCK`
- `SPI2_TX` -> `DMA1_Stream4`

WM8978 控制接口使用软件 IIC。前期验证中已经确认 WM8978 在 7bit 地址 `0x1A` 可以稳定 ACK。

## 整体播放链路

音乐播放可以分成三层：

1. 文件层：通过 FATFS 从 SD 卡读取音乐文件。
2. 解码层：
   - WAV：文件本身就是 PCM，只需要解析 WAV 头并读取 data 数据。
   - MP3：使用 `minimp3` 将 MP3 压缩帧解码成 PCM。
3. 输出层：通过 WM8978 + I2S2 + DMA/阻塞发送，把 PCM 输出到耳机接口。

## WAV 播放实现

相关文件：

- `Library/audio/wav_player.c`
- `Library/audio/wav_player.h`

WAV 播放流程：

1. 打开用户在文件浏览器中选择的 `.wav` 文件。
2. 读取 RIFF/WAVE 文件头。
3. 扫描 WAV chunk：
   - 解析 `fmt ` chunk，得到采样率、声道数、位深等信息。
   - 找到 `data` chunk，记录 PCM 数据起始位置和长度。
   - 对 `LIST` 等额外 chunk 直接跳过。
4. 校验格式是否支持。
5. 初始化 WM8978，并设置当前音量。
6. 循环从 SD 卡读取 PCM 数据。
7. 使用 `HAL_I2S_Transmit()` 把 PCM 直接发送到 I2S2。
8. 播放期间扫描按键，实现音量加减。

当前 WAV 只支持：

- PCM 格式，`audio_format = 1`
- 双声道
- 16bit
- 44100Hz
- `block_align = 4`

这样设计的原因是当前 I2S2 配置固定面向 44.1kHz、16bit、双声道音频。第一阶段不做重采样、声道转换或位深转换，可以减少不确定因素。

## MP3 播放实现

相关文件：

- `Library/audio/mp3_decode_test.c`
- `Library/audio/mp3_decode_test.h`
- `Library/minimp3/minimp3.h`

MP3 播放流程：

1. 打开用户在文件浏览器中选择的 `.mp3` 文件。
2. 读取文件开头字节并打印调试信息。
3. 判断是否存在 ID3v2 标签：
   - 如果存在，按 ID3 synchsafe 长度跳过标签区。
   - 如果不存在，直接从文件开头开始解码。
4. 从 SD 卡分窗口读取 MP3 压缩数据到 `g_mp3_input`。
5. 使用 `minimp3` 解码 MP3 帧，输出 16bit stereo PCM。
6. 将 PCM 填入 I2S DMA 双缓冲。
7. 启动 `HAL_I2S_Transmit_DMA()`。
8. DMA 半传输和全传输回调通知主循环：
   - 前半缓冲播放完，就补前半缓冲。
   - 后半缓冲播放完，就补后半缓冲。
9. 文件解码结束后，用静音数据填满剩余 DMA 缓冲，让播放自然结束。

MP3 当前只支持：

- 44100Hz
- 双声道
- minimp3 能正常识别的 Layer III MP3

如果 MP3 是其他采样率，例如 48kHz、32kHz，当前代码会返回 `UNSUPPORTED`。后续如果要支持更多格式，需要增加重采样，或者播放前根据文件采样率重新配置 I2S 时钟。

## DMA 双缓冲设计

MP3 解码比 WAV 复杂，不能简单地一次解码一点再阻塞发送，否则容易出现声音一顿一顿。

当前 MP3 使用一个 DMA 环形缓冲：

- `g_mp3_dma_buffer` 被分成两个半区。
- I2S DMA 循环播放整个缓冲。
- DMA 播放完前半区时触发 `HAL_I2S_TxHalfCpltCallback()`。
- DMA 播放完后半区时触发 `HAL_I2S_TxCpltCallback()`。
- 主循环看到 ready 标志后，立即把对应半区重新填入新的 PCM。

这样做的好处：

- I2S 输出保持连续。
- 解码和播放可以交替进行。
- 可以通过 `underrun` 计数判断 CPU/SD 卡读取是否跟不上播放。

## 已解决的关键问题

### 1. WM8978 无 ACK

早期 IIC 扫描时只能看到 `0x50`、`0x68` 等设备，WM8978 的 `0x1A` 没有 ACK。

后来通过软件 IIC 调试确认：

- SCL/SDA 拉低和释放电平正常。
- 问题集中在 ACK 阶段 SDA 方向和时序。

修正后 WM8978 可以稳定 ACK，音频硬件链路通过方波 tone 测试验证。

### 2. WAV 文件无法解析

有些扩展名是 `.wav` 的文件，实际开头是 `ID3`，本质是 MP3，不是 RIFF/WAVE。

代码现在会打印文件头：

```text
[WAV] bad RIFF header: ...
```

真正的 WAV 文件会解析出：

```text
[WAV] audioformat:1 channels:2 samplerate:44100 ...
```

### 3. MP3 播放像快进、声音断续

调试日志曾出现：

```text
underrun=0
zero=3489
skip=2033402
```

这说明 DMA 没有断流，问题不在 I2S 输出，而在 MP3 解码流的位置推进。

根因是对 `minimp3` 的 `frame_info.frame_bytes` 理解错误。

`minimp3` 返回的 `frame_bytes` 已经包含 `frame_offset`，原代码曾经写成：

```c
stream->offset += frame_offset + frame_info.frame_bytes;
```

这会重复加一次 `frame_offset`，导致文件位置被额外跳过，声音就会断续、像快进。

修正后：

```c
stream->offset += (uint32_t)frame_info.frame_bytes;
```

MP3 播放恢复连续。

## 串口调试字段说明

MP3 播放结束时会打印：

```text
[MP3P] playback done frames=... pcm_samples=... underrun=... pos=.../... eof=... done=... zero=... skip=...
```

字段含义：

- `frames`：成功输出 PCM 的 MP3 帧数量。
- `pcm_samples`：输出到 I2S DMA 的 PCM word 数。
- `underrun`：DMA 半缓冲来不及填充的次数。
- `pos`：当前解码到的文件位置。
- `eof`：是否已经读到文件末尾。
- `done`：解码流程是否结束。
- `zero`：找到 MP3 帧但没有输出 PCM 的次数。
- `skip`：为了重新同步 MP3 帧而跳过的字节数。

判断方法：

- `underrun > 0`：说明解码或 SD 卡读取速度跟不上 I2S 播放。
- `underrun = 0` 但 `skip` 很大：说明 MP3 同步或文件位置推进有问题。
- `pos` 明显小于文件大小：说明播放循环提前结束。
- `pos` 接近文件大小且声音正常：说明完整读完文件。

## 用户操作方式

1. 上电后等待 SD 卡和字体初始化完成。
2. 按 `KEY0` 进入文件浏览。
3. 用 `KEY1` / `KEY_UP` 上下选择文件或目录。
4. 选中 `Music` 目录后按 `KEY2` 进入。
5. 选中 `.wav` 或 `.mp3` 文件后按 `KEY2` 播放。
6. 播放过程中：
   - `KEY0` 停止当前播放。
   - `KEY1` 降低音量。
   - `KEY2` 暂停/继续。
   - `KEY_UP` 增加音量。

## 当前限制

- WAV 只支持 44.1kHz、16bit、双声道 PCM。
- MP3 当前主要验证 44.1kHz、双声道文件。
- 当前播放是功能验证版本，已经支持播放中停止和暂停/继续，还没有上一首、下一首等完整播放器控制。
- MP3 解码使用软件解码，占用 CPU 较高，因此 `mp3_decode_test.c` 在 CMake 中单独使用 `-O2` 编译。
- I2S 实际采样率仍建议后续继续校准到更接近 44.1kHz。

## 后续可扩展方向

- 增加上一首、下一首。
- 在 LCD 上显示播放状态、文件名、音量。
- 支持更多 WAV 格式。
- 支持不同采样率 MP3，并动态调整 I2S 或加入重采样。
- 将当前测试型 `mp3_decode_test` 重构成正式 `mp3_player` 模块。
