#include "mp3_decode_test.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "../key/key.h"
#include "../wm8978/wm8978.h"

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "../minimp3/minimp3.h"

#define MP3_DECODE_INPUT_BYTES  (24U * 1024U)
#define MP3_DECODE_REFILL_LEVEL  4096U
#define MP3_DMA_HALF_WORDS       (MINIMP3_MAX_SAMPLES_PER_FRAME * 4U)
#define MP3_DMA_TOTAL_WORDS      (MP3_DMA_HALF_WORDS * 2U)
#define MP3_DIAGNOSTICS          0U
#define MP3_VOLUME_MIN           0U
#define MP3_VOLUME_MAX           63U
#define MP3_VOLUME_DEFAULT       30U
#define MP3_VOLUME_STEP          4U
#define MP3_CONTROL_NONE         0U
#define MP3_CONTROL_STOP         1U
#define MP3_CONTROL_PAUSE        2U

/* MP3 播放链路：
 * 1. FATFS 从 SD 卡读取压缩数据到 g_mp3_input。
 * 2. minimp3 将压缩帧解码成 44.1kHz/16bit/stereo PCM。
 * 3. I2S2 DMA 循环发送 g_mp3_dma_buffer，半传输/全传输回调用来通知主循环补下一半 PCM。
 *
 * 这样播放和解码可以并行进行，比阻塞式 HAL_I2S_Transmit 更不容易卡顿。
 */
static uint8_t g_mp3_input[MP3_DECODE_INPUT_BYTES];
static mp3d_sample_t g_mp3_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static uint16_t g_mp3_dma_buffer[MP3_DMA_TOTAL_WORDS];
static mp3dec_t g_mp3_decoder;
static I2S_HandleTypeDef *g_mp3_i2s = NULL;
static uint8_t g_mp3_volume = MP3_VOLUME_DEFAULT;
static uint32_t g_mp3_window_count = 0U;
static volatile uint8_t g_mp3_dma_half_ready = 0U;
static volatile uint8_t g_mp3_dma_full_ready = 0U;
static volatile uint8_t g_mp3_dma_active = 0U;
static volatile uint32_t g_mp3_dma_underrun_count = 0U;

typedef struct
{
    FIL *file;
    uint32_t id3_skip;
    /* 当前输入窗口长度，以及在窗口内已经消费到的位置。 */
    uint32_t data_len;
    uint32_t offset;
    /* 已经从文件流起点消费掉的完整窗口偏移，不包含当前窗口 offset。 */
    uint32_t stream_offset;
    uint8_t eof;
    uint8_t decode_done;
    uint32_t decoded_frames;
    uint32_t decoded_samples;
    /* 调试计数：用于判断是否出现解码同步问题或 DMA 来不及补数据。 */
    uint32_t zero_sample_frames;
    uint32_t sync_skip_bytes;
} Mp3DecodeStream;

/**
 * @brief 计算当前 MP3 解码流对应的文件绝对位置。
 * @param stream MP3 解码流状态。
 * @return 文件偏移，单位字节。
 */
static uint32_t Mp3DecodeTest_StreamFilePos(const Mp3DecodeStream *stream)
{
    return stream->id3_skip + stream->stream_offset + stream->offset;
}

/**
 * @brief 读取 ID3v2 使用的 synchsafe 32bit 长度字段。
 * @param buf 指向 4 字节 synchsafe 数据。
 * @return 解码后的普通 32bit 长度。
 */
static uint32_t Mp3DecodeTest_ReadSynchsafe32(const uint8_t *buf)
{
    return ((uint32_t)(buf[0] & 0x7FU) << 21) |
           ((uint32_t)(buf[1] & 0x7FU) << 14) |
           ((uint32_t)(buf[2] & 0x7FU) << 7) |
           ((uint32_t)(buf[3] & 0x7FU));
}

/**
 * @brief 判断文件头是否存在 ID3v2 标签，并计算需要跳过的字节数。
 * @param buf 文件开头数据。
 * @param len buf 中有效字节数。
 * @return 需要跳过的 ID3 标签长度；没有 ID3v2 时返回 0。
 */
static uint32_t Mp3DecodeTest_GetId3Skip(const uint8_t *buf, uint32_t len)
{
    uint32_t skip;

    if (len < 10U || memcmp(buf, "ID3", 3U) != 0)
    {
        return 0U;
    }

    /* ID3v2 标签长度使用 synchsafe 32bit 编码；解码前必须跳过标签区。 */
    skip = Mp3DecodeTest_ReadSynchsafe32(&buf[6]) + 10UL;
    if ((buf[5] & 0x10U) != 0U)
    {
        skip += 10UL;
    }
    return skip;
}

/**
 * @brief 以十六进制形式打印 MP3 文件头数据。
 * @param tag 串口打印前缀。
 * @param buf 待打印数据缓冲区。
 * @param len 待打印字节数。
 * @return 无。
 */
static void Mp3DecodeTest_PrintBytes(const char *tag, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    printf("[MP3D] %s", tag);
    for (i = 0U; i < len; i++)
    {
        printf(" %02X", (unsigned int)buf[i]);
    }
    printf("\r\n");
}

/**
 * @brief 将当前 MP3 音量写入 WM8978 耳机和喇叭输出。
 * @param 无。
 * @return 无。
 */
static void Mp3DecodeTest_ApplyVolume(void)
{
    (void)WM8978_SetHPVolume(g_mp3_volume, g_mp3_volume);
    (void)WM8978_SetSPKVolume(g_mp3_volume);
}

/**
 * @brief 扫描 MP3 播放过程中的音量按键。
 * @param 无。
 * @return 无。
 * @note KEY1 降低音量，KEY_UP 增加音量；音量变化后立即写入 WM8978。
 */
/**
 * @brief 扫描 MP3 播放过程中的控制按键。
 * @param 无。
 * @return MP3_CONTROL_STOP 表示停止；MP3_CONTROL_PAUSE 表示暂停/继续；MP3_CONTROL_NONE 表示继续播放。
 * @note KEY0 停止播放，KEY2 暂停/继续，KEY1 降低音量，KEY_UP 增加音量。
 */
static uint8_t Mp3DecodeTest_HandlePlaybackKey(void)
{
    KeyName_t key = Key_Scan();
    uint8_t old_volume = g_mp3_volume;

    if (key == KEY0)
    {
        printf("[MP3P] stop requested by KEY0\r\n");
        return MP3_CONTROL_STOP;
    }
    else if (key == KEY2)
    {
        return MP3_CONTROL_PAUSE;
    }
    else if (key == KEY1)
    {
        if (g_mp3_volume > MP3_VOLUME_STEP)
        {
            g_mp3_volume = (uint8_t)(g_mp3_volume - MP3_VOLUME_STEP);
        }
        else
        {
            g_mp3_volume = MP3_VOLUME_MIN;
        }
    }
    else if (key == KEY_UP)
    {
        if (g_mp3_volume < (MP3_VOLUME_MAX - MP3_VOLUME_STEP))
        {
            g_mp3_volume = (uint8_t)(g_mp3_volume + MP3_VOLUME_STEP);
        }
        else
        {
            g_mp3_volume = MP3_VOLUME_MAX;
        }
    }

    if (g_mp3_volume != old_volume)
    {
        Mp3DecodeTest_ApplyVolume();
        printf("[MP3P] volume=%u\r\n", (unsigned int)g_mp3_volume);
    }

    return MP3_CONTROL_NONE;
}

/**
 * @brief I2S DMA 半传输完成回调。
 * @param hi2s 触发回调的 I2S 句柄。
 * @return 无。
 * @note 当前用于通知主循环补充 DMA 前半缓冲。
 */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == g_mp3_i2s && g_mp3_dma_active != 0U)
    {
        /* 如果上一次 half-ready 标志还没被主循环清掉，说明补数据速度已经落后 DMA。 */
        if (g_mp3_dma_half_ready != 0U)
        {
            g_mp3_dma_underrun_count++;
        }
        g_mp3_dma_half_ready = 1U;
    }
}

/**
 * @brief I2S DMA 全传输完成回调。
 * @param hi2s 触发回调的 I2S 句柄。
 * @return 无。
 * @note 当前用于通知主循环补充 DMA 后半缓冲。
 */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == g_mp3_i2s && g_mp3_dma_active != 0U)
    {
        /* full-ready 同理，用 underrun 计数观察是否存在播放断流。 */
        if (g_mp3_dma_full_ready != 0U)
        {
            g_mp3_dma_underrun_count++;
        }
        g_mp3_dma_full_ready = 1U;
    }
}

/**
 * @brief 从 MP3 文件指定位置读取一个输入窗口。
 * @param file 已打开的 FATFS 文件对象。
 * @param file_offset 文件读取起始偏移。
 * @param data_len 输出实际读取到的字节数。
 * @param eof 输出是否已经读到文件末尾，1 表示是，0 表示否。
 * @return MP3_DECODE_TEST_OK 表示读取成功；否则返回 MP3_DECODE_TEST_ERR_READ。
 */
static Mp3DecodeTestResult Mp3DecodeTest_ReadWindow(FIL *file,
                                                    uint32_t file_offset,
                                                    uint32_t *data_len,
                                                    uint8_t *eof)
{
    FRESULT fres;
    UINT br = 0U;

    /* STM32F407 RAM 有限，不能把整个 MP3 读入内存，只能按窗口分段读取。 */
    if (f_lseek(file, file_offset) != FR_OK)
    {
        return MP3_DECODE_TEST_ERR_READ;
    }

    fres = f_read(file, g_mp3_input, MP3_DECODE_INPUT_BYTES, &br);
    if (fres != FR_OK)
    {
        return MP3_DECODE_TEST_ERR_READ;
    }

    *data_len = (uint32_t)br;
    *eof = (br < MP3_DECODE_INPUT_BYTES) ? 1U : 0U;
    g_mp3_window_count++;
    if (MP3_DIAGNOSTICS != 0U &&
        br > 0U &&
        (g_mp3_window_count <= 3U || (g_mp3_window_count % 100U) == 0U))
    {
        printf("[MP3P] read window offset=%lu bytes=%u eof=%u\r\n",
               (unsigned long)file_offset,
               (unsigned int)br,
               (unsigned int)*eof);
    }

    return MP3_DECODE_TEST_OK;
}

/**
 * @brief 根据当前流位置加载下一段 MP3 输入窗口。
 * @param stream MP3 解码流状态。
 * @return MP3_DECODE_TEST_OK 表示加载成功；否则返回读取错误。
 * @note 会更新 stream_offset、offset、data_len、eof 和 decode_done。
 */
static Mp3DecodeTestResult Mp3DecodeTest_LoadNextWindow(Mp3DecodeStream *stream)
{
    Mp3DecodeTestResult ret;

    /* 当前窗口剩余数据不足时，从“文件流绝对位置”继续读取下一窗口。 */
    stream->stream_offset += stream->offset;
    stream->offset = 0U;
    ret = Mp3DecodeTest_ReadWindow(stream->file,
                                   stream->id3_skip + stream->stream_offset,
                                   &stream->data_len,
                                   &stream->eof);
    if (ret != MP3_DECODE_TEST_OK)
    {
        return ret;
    }
    if (stream->data_len == 0U)
    {
        stream->decode_done = 1U;
    }
    return MP3_DECODE_TEST_OK;
}

/**
 * @brief 解码 MP3 数据并填满一个 I2S DMA 半缓冲。
 * @param stream MP3 解码流状态。
 * @param dst 目标 DMA 半缓冲地址。
 * @param words 需要填充的 16bit PCM word 数。
 * @return MP3_DECODE_TEST_OK 表示填充成功；其他值表示解码、读取或格式不支持。
 * @note 文件结束后会用 0 填充剩余缓冲，避免尾部输出旧数据。
 */
static Mp3DecodeTestResult Mp3DecodeTest_FillDmaHalf(Mp3DecodeStream *stream,
                                                     uint16_t *dst,
                                                     uint32_t words)
{
    uint32_t written = 0U;

    /* 填满一个 DMA 半缓冲。若文件已经解码结束，则用 0 填充，让尾音自然结束。 */
    while (written < words)
    {
        uint32_t bytes_left;
        mp3dec_frame_info_t frame_info;
        uint32_t frame_offset;
        int samples;

        if (stream->decode_done != 0U)
        {
            memset(&dst[written], 0, (words - written) * sizeof(dst[0]));
            return MP3_DECODE_TEST_OK;
        }

        bytes_left = stream->data_len - stream->offset;
        if (bytes_left == 0U)
        {
            if (stream->eof != 0U)
            {
                stream->decode_done = 1U;
                continue;
            }
            return Mp3DecodeTest_LoadNextWindow(stream);
        }

        memset(&frame_info, 0, sizeof(frame_info));
        samples = mp3dec_decode_frame(&g_mp3_decoder,
                                      &g_mp3_input[stream->offset],
                                      (int)bytes_left,
                                      g_mp3_pcm,
                                      &frame_info);

        if (frame_info.frame_bytes <= 0)
        {
            if ((stream->offset + 1U) < stream->data_len)
            {
                stream->offset++;
                continue;
            }
            if (stream->eof != 0U)
            {
                stream->decode_done = 1U;
                continue;
            }
            return Mp3DecodeTest_LoadNextWindow(stream);
        }

        if (frame_info.frame_offset < 0)
        {
            return MP3_DECODE_TEST_ERR_DECODE;
        }
        frame_offset = (uint32_t)frame_info.frame_offset;
        stream->sync_skip_bytes += frame_offset;

        if (samples > 0)
        {
            uint32_t pcm_words = (uint32_t)samples * (uint32_t)frame_info.channels;

            /* 当前 I2S 配置固定为 44.1kHz 双声道；其他采样率需要后续增加重采样或重配 I2S。 */
            if (frame_info.hz != 44100 || frame_info.channels != 2)
            {
                printf("[MP3P] unsupported stream hz=%d ch=%d, need 44100Hz stereo\r\n",
                       frame_info.hz,
                       frame_info.channels);
                return MP3_DECODE_TEST_ERR_UNSUPPORTED;
            }

            if ((written + pcm_words) > words)
            {
                printf("[MP3P] dma half too small pcm_words=%lu remain=%lu\r\n",
                       (unsigned long)pcm_words,
                       (unsigned long)(words - written));
                return MP3_DECODE_TEST_ERR_DECODE;
            }

            /* minimp3 输出的是交错的 L/R 16bit PCM，正好可以直接放进 I2S DMA 缓冲。 */
            memcpy(&dst[written], g_mp3_pcm, pcm_words * sizeof(dst[0]));
            written += pcm_words;
            stream->decoded_frames++;
            stream->decoded_samples += pcm_words;

            if (stream->decoded_frames <= 2U || (stream->decoded_frames % 2000U) == 0U)
            {
                printf("[MP3P] frame %lu hz=%d ch=%d bitrate=%d kbps\r\n",
                       (unsigned long)stream->decoded_frames,
                       frame_info.hz,
                       frame_info.channels,
                       frame_info.bitrate_kbps);
            }
        }
        else
        {
            stream->zero_sample_frames++;
            if (stream->zero_sample_frames <= 3U || (stream->zero_sample_frames % 100U) == 0U)
            {
                printf("[MP3P] zero frame %lu offset=%lu bytes=%d skip=%lu\r\n",
                       (unsigned long)stream->zero_sample_frames,
                       (unsigned long)Mp3DecodeTest_StreamFilePos(stream),
                       frame_info.frame_bytes,
                       (unsigned long)frame_offset);
            }
        }

        /* 注意：minimp3 的 frame_bytes 已经包含 frame_offset。
         * 不能再额外加 frame_offset，否则会重复跳过数据，声音会断续并像快进。
         */
        stream->offset += (uint32_t)frame_info.frame_bytes;
        if (stream->offset >= stream->data_len)
        {
            stream->offset = stream->data_len;
        }
        if ((stream->data_len - stream->offset) < MP3_DECODE_REFILL_LEVEL && stream->eof == 0U)
        {
            Mp3DecodeTestResult ret = Mp3DecodeTest_LoadNextWindow(stream);
            if (ret != MP3_DECODE_TEST_OK)
            {
                return ret;
            }
        }
    }

    return MP3_DECODE_TEST_OK;
}

/**
 * @brief 注册 MP3 播放使用的 I2S 句柄。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 无。
 * @note 必须在 Mp3DecodeTest_File 调用前注册，否则返回 MP3_DECODE_TEST_ERR_NO_I2S。
 */
void Mp3DecodeTest_SetI2SHandle(I2S_HandleTypeDef *hi2s)
{
    g_mp3_i2s = hi2s;
}

/**
 * @brief 播放一个 MP3 文件。
 * @param path FATFS 文件路径，例如 "0:/Music/test.mp3"。
 * @return MP3_DECODE_TEST_OK 表示播放结束；其他值表示打开、读取、解码、WM8978 或 I2S 错误。
 * @note 当前只支持 44.1kHz 双声道 MP3；播放期间 KEY1/KEY_UP 可调节音量。
 */
Mp3DecodeTestResult Mp3DecodeTest_File(const char *path)
{
    FIL file;
    FRESULT fres;
    UINT br = 0U;
    uint32_t file_size;
    uint32_t id3_skip = 0U;
    uint8_t silence_halves = 0U;
    uint8_t paused = 0U;
    Mp3DecodeStream stream;
    Mp3DecodeTestResult ret;

    if (g_mp3_i2s == NULL)
    {
        return MP3_DECODE_TEST_ERR_NO_I2S;
    }

    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        printf("[MP3D] open failed path=%s res=%u\r\n", path, (unsigned int)fres);
        return MP3_DECODE_TEST_ERR_OPEN;
    }

    file_size = (uint32_t)f_size(&file);
    printf("[MP3D] open: %s\r\n", path);
    printf("[MP3D] size=%lu\r\n", (unsigned long)file_size);

    fres = f_read(&file, g_mp3_input, 16U, &br);
    if (fres != FR_OK || br == 0U)
    {
        (void)f_close(&file);
        return MP3_DECODE_TEST_ERR_READ;
    }
    Mp3DecodeTest_PrintBytes("first bytes:", g_mp3_input, br);

    id3_skip = Mp3DecodeTest_GetId3Skip(g_mp3_input, br);
    if (id3_skip > 0U)
    {
        printf("[MP3D] ID3 tag found, skip=%lu\r\n", (unsigned long)id3_skip);
    }
    else
    {
        printf("[MP3D] ID3 tag not found\r\n");
    }

    if (f_lseek(&file, id3_skip) != FR_OK)
    {
        (void)f_close(&file);
        return MP3_DECODE_TEST_ERR_READ;
    }

    memset(&stream, 0, sizeof(stream));
    stream.file = &file;
    stream.id3_skip = id3_skip;

    g_mp3_window_count = 0U;
    ret = Mp3DecodeTest_ReadWindow(&file, stream.id3_skip, &stream.data_len, &stream.eof);
    if (ret != MP3_DECODE_TEST_OK || stream.data_len == 0U)
    {
        (void)f_close(&file);
        return MP3_DECODE_TEST_ERR_READ;
    }

    (void)HAL_I2S_DMAStop(g_mp3_i2s);
    if (WM8978_Init() != 0U)
    {
        (void)f_close(&file);
        return MP3_DECODE_TEST_ERR_CODEC;
    }
    Mp3DecodeTest_ApplyVolume();
    printf("[MP3P] volume=%u KEY1:- KEY_UP:+\r\n", (unsigned int)g_mp3_volume);

    mp3dec_init(&g_mp3_decoder);

    /* 先预填两个半缓冲，再启动 DMA，避免刚开始播放就读到空数据。 */
    ret = Mp3DecodeTest_FillDmaHalf(&stream, &g_mp3_dma_buffer[0], MP3_DMA_HALF_WORDS);
    if (ret != MP3_DECODE_TEST_OK)
    {
        (void)f_close(&file);
        return ret;
    }
    ret = Mp3DecodeTest_FillDmaHalf(&stream, &g_mp3_dma_buffer[MP3_DMA_HALF_WORDS], MP3_DMA_HALF_WORDS);
    if (ret != MP3_DECODE_TEST_OK)
    {
        (void)f_close(&file);
        return ret;
    }

    g_mp3_dma_half_ready = 0U;
    g_mp3_dma_full_ready = 0U;
    g_mp3_dma_underrun_count = 0U;
    g_mp3_dma_active = 1U;
    printf("[MP3P] playback start: %s\r\n", path);

    if (HAL_I2S_Transmit_DMA(g_mp3_i2s, g_mp3_dma_buffer, MP3_DMA_TOTAL_WORDS) != HAL_OK)
    {
        g_mp3_dma_active = 0U;
        (void)f_close(&file);
        printf("[MP3P] I2S DMA start failed err=0x%08lX\r\n",
               (unsigned long)HAL_I2S_GetError(g_mp3_i2s));
        return MP3_DECODE_TEST_ERR_I2S;
    }

    /* DMA 循环播放两个半缓冲。哪个半缓冲播放完，主循环就解码并补哪个半缓冲。 */
    while (silence_halves < 2U)
    {
        if (g_mp3_dma_half_ready != 0U)
        {
            g_mp3_dma_half_ready = 0U;
            ret = Mp3DecodeTest_FillDmaHalf(&stream, &g_mp3_dma_buffer[0], MP3_DMA_HALF_WORDS);
            if (ret != MP3_DECODE_TEST_OK)
            {
                break;
            }
            if (stream.decode_done != 0U)
            {
                silence_halves++;
            }
        }

        if (g_mp3_dma_full_ready != 0U)
        {
            g_mp3_dma_full_ready = 0U;
            ret = Mp3DecodeTest_FillDmaHalf(&stream, &g_mp3_dma_buffer[MP3_DMA_HALF_WORDS], MP3_DMA_HALF_WORDS);
            if (ret != MP3_DECODE_TEST_OK)
            {
                break;
            }
            if (stream.decode_done != 0U)
            {
                silence_halves++;
            }
        }

        uint8_t control = Mp3DecodeTest_HandlePlaybackKey();
        if (control == MP3_CONTROL_STOP)
        {
            ret = MP3_DECODE_TEST_STOPPED;
            break;
        }
        if (control == MP3_CONTROL_PAUSE)
        {
            paused = 1U;
            (void)HAL_I2S_DMAPause(g_mp3_i2s);
            printf("[MP3P] playback paused, KEY2:resume KEY0:stop\r\n");
        }

        while (paused != 0U)
        {
            control = Mp3DecodeTest_HandlePlaybackKey();
            if (control == MP3_CONTROL_STOP)
            {
                ret = MP3_DECODE_TEST_STOPPED;
                paused = 0U;
                break;
            }
            if (control == MP3_CONTROL_PAUSE)
            {
                paused = 0U;
                (void)HAL_I2S_DMAResume(g_mp3_i2s);
                printf("[MP3P] playback resumed\r\n");
            }
            HAL_Delay(10U);
        }

        if (ret != MP3_DECODE_TEST_OK)
        {
            break;
        }
    }

    g_mp3_dma_active = 0U;
    (void)HAL_I2S_DMAStop(g_mp3_i2s);
    (void)f_close(&file);

    if (ret != MP3_DECODE_TEST_OK)
    {
        printf("[MP3P] playback stop ret=%s frames=%lu pcm_samples=%lu underrun=%lu pos=%lu/%lu eof=%u done=%u zero=%lu skip=%lu\r\n",
               Mp3DecodeTest_ResultString(ret),
               (unsigned long)stream.decoded_frames,
               (unsigned long)stream.decoded_samples,
               (unsigned long)g_mp3_dma_underrun_count,
               (unsigned long)Mp3DecodeTest_StreamFilePos(&stream),
               (unsigned long)file_size,
               (unsigned int)stream.eof,
               (unsigned int)stream.decode_done,
               (unsigned long)stream.zero_sample_frames,
               (unsigned long)stream.sync_skip_bytes);
        return ret;
    }

    if (stream.decoded_frames == 0U)
    {
        printf("[MP3P] no decodable frame\r\n");
        return MP3_DECODE_TEST_ERR_NO_FRAME;
    }

    printf("[MP3P] playback done frames=%lu pcm_samples=%lu underrun=%lu pos=%lu/%lu eof=%u done=%u zero=%lu skip=%lu\r\n",
           (unsigned long)stream.decoded_frames,
           (unsigned long)stream.decoded_samples,
           (unsigned long)g_mp3_dma_underrun_count,
           (unsigned long)Mp3DecodeTest_StreamFilePos(&stream),
           (unsigned long)file_size,
           (unsigned int)stream.eof,
           (unsigned int)stream.decode_done,
           (unsigned long)stream.zero_sample_frames,
           (unsigned long)stream.sync_skip_bytes);
    return MP3_DECODE_TEST_OK;
}

/**
 * @brief 将 MP3 播放结果转换为便于串口/LCD 显示的字符串。
 * @param result Mp3DecodeTest_File 返回的结果码。
 * @return 指向静态字符串的指针。
 */
const char *Mp3DecodeTest_ResultString(Mp3DecodeTestResult result)
{
    switch (result)
    {
        case MP3_DECODE_TEST_OK: return "OK";
        case MP3_DECODE_TEST_ERR_OPEN: return "OPEN";
        case MP3_DECODE_TEST_ERR_READ: return "READ";
        case MP3_DECODE_TEST_ERR_NO_FRAME: return "NO_FRAME";
        case MP3_DECODE_TEST_ERR_DECODE: return "DECODE";
        case MP3_DECODE_TEST_ERR_NO_I2S: return "NO_I2S";
        case MP3_DECODE_TEST_ERR_CODEC: return "CODEC";
        case MP3_DECODE_TEST_ERR_UNSUPPORTED: return "UNSUPPORTED";
        case MP3_DECODE_TEST_ERR_I2S: return "I2S";
        case MP3_DECODE_TEST_STOPPED: return "STOPPED";
        default: return "UNKNOWN";
    }
}
