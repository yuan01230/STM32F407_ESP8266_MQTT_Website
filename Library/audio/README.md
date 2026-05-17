# audio 音频模块说明

## 模块目标

`Library/audio` 目录负责把 SD 卡中的音频文件转换成 I2S 可播放的 PCM 数据，并通过 WM8978 输出到耳机或喇叭接口。

当前已经实现：

- 方波测试音：验证 WM8978、I2S2、DMA 和耳机输出链路。
- WAV 播放：支持 44.1kHz、16bit、双声道 PCM WAV。
- MP3 探测：识别 MP3 文件头、ID3 标签和首个 MP3 帧。
- MP3 播放：使用 `minimp3` 软件解码，I2S DMA 双缓冲输出。
- 播放中音量控制：`KEY1` 降音量，`KEY_UP` 加音量。
- 播放中停止控制：`KEY0` 停止当前 WAV/MP3 播放。
- 播放中暂停控制：`KEY2` 暂停/继续当前 WAV/MP3 播放。

## 文件分工

| 文件 | 作用 |
| --- | --- |
| `audio_test.c/.h` | 音频硬件验证，输出固定方波 tone |
| `wav_player.c/.h` | WAV 解析和 PCM 播放 |
| `mp3_probe.c/.h` | MP3 文件探测，不解码、不播放 |
| `mp3_decode_test.c/.h` | MP3 解码和 I2S DMA 播放 |
| `../minimp3/minimp3.h` | 第三方 MP3 软件解码器 |
| `../wm8978/wm8978.c/.h` | WM8978 音频芯片控制 |

## 功能实现思路

### 1. 先验证硬件链路

MP3 播放涉及 SD 卡、文件系统、软件解码、I2S、DMA 和音频芯片。直接做 MP3 播放很难判断问题在哪里。

因此先用 `AudioTest_StartTone()` 生成固定 PCM 方波，通过 `HAL_I2S_Transmit_DMA()` 循环输出。只要耳机能听到稳定 tone，就说明：

- WM8978 控制口可写。
- I2S2 时钟和数据管脚正确。
- DMA 能持续搬运数据。
- 耳机输出硬件链路可用。

### 2. WAV 作为 PCM 播放验证

WAV 的 data 区本身就是 PCM，不需要复杂解码。`WavPlayer_PlayFile()` 只做：

1. 打开文件。
2. 解析 RIFF/WAVE。
3. 找到 `fmt ` 和 `data` chunk。
4. 校验格式是否为 44.1kHz、16bit、双声道 PCM。
5. 分块读取 data 区。
6. 使用 `HAL_I2S_Transmit()` 发送到 I2S2。

这一步证明“SD 卡读取真实音频文件 -> I2S 输出”的路径是可用的。

### 3. MP3 分阶段验证

MP3 文件不是 PCM，需要先解码。

早期使用 `Mp3Probe_File()` 做只读探测：

- 打开 MP3 文件。
- 打印文件大小和前 16 字节。
- 如果有 ID3v2 标签，计算并跳过标签长度。
- 在有限窗口内查找第一个合法 MP3 帧头。

这样可以确认文件读取和 MP3 基本结构正常。

### 4. MP3 解码和播放

`Mp3DecodeTest_File()` 使用 `minimp3` 把 MP3 帧解码成 PCM，然后送到 I2S DMA。

核心流程：

1. 打开 MP3 文件。
2. 跳过 ID3 标签。
3. 从 SD 卡读取一段压缩数据窗口到 `g_mp3_input`。
4. 调用 `mp3dec_decode_frame()` 解码一帧。
5. 将输出 PCM 复制到 `g_mp3_dma_buffer`。
6. 启动 `HAL_I2S_Transmit_DMA()`。
7. DMA 半传输/全传输回调设置 ready 标志。
8. 主循环根据 ready 标志补充对应半缓冲。

## 函数调用思路

### 初始化调用链

`Core/Src/main.c` 启动后注册 I2S2：

```c
WavPlayer_SetI2SHandle(&hi2s2);
Mp3DecodeTest_SetI2SHandle(&hi2s2);
```

这一步让 WAV/MP3 模块持有 I2S2 句柄，后续播放时无需直接依赖 `main.c` 的全局变量。

### WAV 播放调用链

```text
UI_View_Show()
  -> WavPlayer_IsWavExtension()
  -> WavPlayer_PlayFile()
       -> WavPlayer_ParseHeader()
       -> WavPlayer_IsSupported()
       -> WM8978_Init()
       -> WavPlayer_ApplyVolume()
       -> f_read()
       -> HAL_I2S_Transmit()
       -> WavPlayer_HandlePlaybackKey()
```

### MP3 播放调用链

```text
UI_View_Show()
  -> Mp3Probe_IsMp3Extension()
  -> Mp3DecodeTest_File()
       -> Mp3DecodeTest_GetId3Skip()
       -> Mp3DecodeTest_ReadWindow()
       -> WM8978_Init()
       -> mp3dec_init()
       -> Mp3DecodeTest_FillDmaHalf()
            -> mp3dec_decode_frame()
       -> HAL_I2S_Transmit_DMA()
       -> HAL_I2S_TxHalfCpltCallback()
       -> HAL_I2S_TxCpltCallback()
       -> Mp3DecodeTest_HandlePlaybackKey()
```

## MP3 DMA 双缓冲

MP3 解码需要 CPU 时间。如果用阻塞发送，容易出现播放一段、停一段的感觉。

当前实现使用 DMA 双缓冲：

- `g_mp3_dma_buffer` 分成前半区和后半区。
- DMA 播放前半区时，主循环可以补后半区。
- DMA 播放后半区时，主循环可以补前半区。
- 如果 ready 标志还没处理又来了下一次回调，`underrun` 加一，用来判断是否补数据太慢。

## 调试字段

MP3 播放结束时会打印：

```text
[MP3P] playback done frames=... pcm_samples=... underrun=... pos=.../... eof=... done=... zero=... skip=...
```

字段含义：

- `frames`：成功输出 PCM 的 MP3 帧数。
- `pcm_samples`：输出的 PCM word 数。
- `underrun`：DMA 半缓冲未及时补充的次数。
- `pos`：当前读取到的文件位置。
- `eof`：是否读到文件末尾。
- `done`：解码是否结束。
- `zero`：解码器找到帧但没有输出 PCM 的次数。
- `skip`：重新同步 MP3 帧时跳过的字节数。

## 当前限制

- WAV 只支持 44.1kHz、16bit、双声道 PCM。
- MP3 当前按 44.1kHz、双声道播放路径验证。
- 当前播放流程是功能验证版，已支持 KEY0 停止播放和 KEY2 暂停/继续；上一首/下一首还未实现。
- `mp3_decode_test.c` 使用 `-O2` 编译，以保证软件解码速度。
