#include "wav_player.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "../key/key.h"
#include "../wm8978/wm8978.h"

#define WAV_READ_CHUNK_BYTES 4096U
#define WAV_READ_CHUNK_WORDS (WAV_READ_CHUNK_BYTES / 2U)
#define WAV_VOLUME_MIN       0U
#define WAV_VOLUME_MAX       63U
#define WAV_VOLUME_DEFAULT   30U
#define WAV_VOLUME_STEP      4U
#define WAV_CONTROL_NONE     0U
#define WAV_CONTROL_STOP     1U
#define WAV_CONTROL_PAUSE    2U

/* WAV 播放走的是最直接的 PCM 通路：
 * SD 卡读取 RIFF/WAVE 文件 -> 校验格式 -> 读 data 块 -> I2S 发送给 WM8978。
 * 当前只支持 44.1kHz、16bit、双声道 PCM，和 CubeMX 中 I2S2 的音频配置保持一致。
 */
static I2S_HandleTypeDef *g_wav_i2s = NULL;
static uint16_t g_wav_pcm_buffer[WAV_READ_CHUNK_WORDS];
static uint8_t g_wav_volume = WAV_VOLUME_DEFAULT;

/**
 * @brief 从缓冲区读取 little-endian 16bit 数值。
 * @param buf 指向至少 2 字节数据的指针。
 * @return 解析后的 16bit 数值。
 */
static uint16_t WavPlayer_ReadLe16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

/**
 * @brief 从缓冲区读取 little-endian 32bit 数值。
 * @param buf 指向至少 4 字节数据的指针。
 * @return 解析后的 32bit 数值。
 */
static uint32_t WavPlayer_ReadLe32(const uint8_t *buf)
{
    return ((uint32_t)buf[0]) |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

/**
 * @brief 以十六进制形式打印一段 WAV 文件数据。
 * @param tag 串口打印前缀。
 * @param buf 待打印数据缓冲区。
 * @param len 待打印字节数。
 * @return 无。
 */
static void WavPlayer_PrintBytes(const char *tag, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    printf("[WAV] %s", tag);
    for (i = 0U; i < len; i++)
    {
        printf(" %02X", (unsigned int)buf[i]);
    }
    printf("\r\n");
}

/**
 * @brief 不区分大小写比较两个扩展名字符。
 * @param a 第一个字符。
 * @param b 第二个字符。
 * @return 1 表示相等；0 表示不相等。
 */
static uint8_t WavPlayer_ExtCharEqual(char a, char b)
{
    if (a >= 'A' && a <= 'Z')
    {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z')
    {
        b = (char)(b - 'A' + 'a');
    }
    return (uint8_t)(a == b);
}

/**
 * @brief 从 FATFS 文件中读取指定长度数据。
 * @param file 已打开的 FATFS 文件对象。
 * @param buf 输出缓冲区。
 * @param len 期望读取的字节数。
 * @return WAV_PLAYER_OK 表示读取长度完全匹配；否则返回 WAV_PLAYER_ERR_READ。
 */
static WavPlayerResult WavPlayer_ReadExact(FIL *file, uint8_t *buf, UINT len)
{
    FRESULT fres;
    UINT br = 0U;

    fres = f_read(file, buf, len, &br);
    if (fres != FR_OK || br != len)
    {
        return WAV_PLAYER_ERR_READ;
    }

    return WAV_PLAYER_OK;
}

/**
 * @brief 解析 WAV 文件头并定位 data chunk。
 * @param file 已打开且位于文件开头的 FATFS 文件对象。
 * @param info 输出 WAV 格式信息和 PCM 数据位置。
 * @return WAV_PLAYER_OK 表示解析成功；其他值表示读取失败或格式不符合 RIFF/WAVE。
 * @note 该函数会跳过 LIST/fact 等非播放数据 chunk。
 */
static WavPlayerResult WavPlayer_ParseHeader(FIL *file, WavInfo *info)
{
    uint8_t riff[12];
    uint8_t chunk_header[8];
    uint8_t fmt_buf[16];
    uint8_t found_fmt = 0U;
    uint8_t found_data = 0U;
    WavPlayerResult ret;

    memset(info, 0, sizeof(*info));

    ret = WavPlayer_ReadExact(file, riff, sizeof(riff));
    if (ret != WAV_PLAYER_OK)
    {
        return ret;
    }

    if (memcmp(&riff[0], "RIFF", 4U) != 0 || memcmp(&riff[8], "WAVE", 4U) != 0)
    {
        WavPlayer_PrintBytes("bad RIFF header:", riff, sizeof(riff));
        return WAV_PLAYER_ERR_FORMAT;
    }

    /* WAV 文件中除了 fmt/data，可能还会有 LIST、fact 等扩展 chunk。
     * 这里按 RIFF chunk 结构逐段扫描，只关心 fmt 和 data，其余 chunk 直接跳过。
     */
    while (found_data == 0U)
    {
        uint32_t chunk_size;
        FSIZE_t next_chunk_offset;

        ret = WavPlayer_ReadExact(file, chunk_header, sizeof(chunk_header));
        if (ret != WAV_PLAYER_OK)
        {
            return ret;
        }

        chunk_size = WavPlayer_ReadLe32(&chunk_header[4]);
        next_chunk_offset = f_tell(file) + (FSIZE_t)chunk_size + (FSIZE_t)(chunk_size & 1U);

        if (memcmp(&chunk_header[0], "fmt ", 4U) == 0)
        {
            if (chunk_size < sizeof(fmt_buf))
            {
                WavPlayer_PrintBytes("short fmt chunk header:", chunk_header, sizeof(chunk_header));
                return WAV_PLAYER_ERR_FORMAT;
            }

            ret = WavPlayer_ReadExact(file, fmt_buf, sizeof(fmt_buf));
            if (ret != WAV_PLAYER_OK)
            {
                return ret;
            }

            info->audio_format = WavPlayer_ReadLe16(&fmt_buf[0]);
            info->channels = WavPlayer_ReadLe16(&fmt_buf[2]);
            info->sample_rate = WavPlayer_ReadLe32(&fmt_buf[4]);
            info->byte_rate = WavPlayer_ReadLe32(&fmt_buf[8]);
            info->block_align = WavPlayer_ReadLe16(&fmt_buf[12]);
            info->bits_per_sample = WavPlayer_ReadLe16(&fmt_buf[14]);
            found_fmt = 1U;
        }
        else if (memcmp(&chunk_header[0], "data", 4U) == 0)
        {
            if (found_fmt == 0U)
            {
                WavPlayer_PrintBytes("data before fmt chunk:", chunk_header, sizeof(chunk_header));
                return WAV_PLAYER_ERR_FORMAT;
            }

            info->data_offset = (uint32_t)f_tell(file);
            info->data_size = chunk_size;
            found_data = 1U;
        }

        if (found_data == 0U)
        {
            printf("[WAV] skip chunk '%c%c%c%c' size=%lu\r\n",
                   chunk_header[0],
                   chunk_header[1],
                   chunk_header[2],
                   chunk_header[3],
                   (unsigned long)chunk_size);
            if (f_lseek(file, next_chunk_offset) != FR_OK)
            {
                return WAV_PLAYER_ERR_READ;
            }
        }
    }

    return WAV_PLAYER_OK;
}

/**
 * @brief 判断当前 WAV 参数是否可由现有 I2S 配置直接播放。
 * @param info WAV 文件格式信息。
 * @return 1 表示支持；0 表示不支持。
 * @note 当前只支持 PCM、44.1kHz、16bit、双声道、block_align=4。
 */
static uint8_t WavPlayer_IsSupported(const WavInfo *info)
{
    /* 第一阶段不做重采样和声道转换，只播放和 I2S2 配置完全一致的 PCM。 */
    if (info->audio_format != 1U)
    {
        return 0U;
    }
    if (info->channels != 2U)
    {
        return 0U;
    }
    if (info->bits_per_sample != 16U)
    {
        return 0U;
    }
    if (info->sample_rate != 44100UL)
    {
        return 0U;
    }
    if (info->block_align != 4U)
    {
        return 0U;
    }
    return 1U;
}

/**
 * @brief 将当前 WAV 音量写入 WM8978 耳机和喇叭输出。
 * @param 无。
 * @return 无。
 */
static void WavPlayer_ApplyVolume(void)
{
    (void)WM8978_SetHPVolume(g_wav_volume, g_wav_volume);
    (void)WM8978_SetSPKVolume(g_wav_volume);
}

/**
 * @brief 扫描播放过程中的音量按键并更新 WM8978 音量。
 * @param 无。
 * @return 无。
 * @note KEY1 降低音量，KEY_UP 增加音量。
 */
/**
 * @brief 扫描 WAV 播放过程中的控制按键。
 * @param 无。
 * @return WAV_CONTROL_STOP 表示停止；WAV_CONTROL_PAUSE 表示暂停/继续；WAV_CONTROL_NONE 表示继续播放。
 * @note KEY0 停止播放，KEY2 暂停/继续，KEY1 降低音量，KEY_UP 增加音量。
 */
static uint8_t WavPlayer_HandlePlaybackKey(void)
{
    KeyName_t key = Key_Scan();
    uint8_t old_volume = g_wav_volume;

    if (key == KEY0)
    {
        printf("[WAV] stop requested by KEY0\r\n");
        return WAV_CONTROL_STOP;
    }
    else if (key == KEY2)
    {
        return WAV_CONTROL_PAUSE;
    }
    else if (key == KEY1)
    {
        if (g_wav_volume > WAV_VOLUME_STEP)
        {
            g_wav_volume = (uint8_t)(g_wav_volume - WAV_VOLUME_STEP);
        }
        else
        {
            g_wav_volume = WAV_VOLUME_MIN;
        }
    }
    else if (key == KEY_UP)
    {
        if (g_wav_volume < (WAV_VOLUME_MAX - WAV_VOLUME_STEP))
        {
            g_wav_volume = (uint8_t)(g_wav_volume + WAV_VOLUME_STEP);
        }
        else
        {
            g_wav_volume = WAV_VOLUME_MAX;
        }
    }

    if (g_wav_volume != old_volume)
    {
        WavPlayer_ApplyVolume();
        printf("[WAV] volume=%u\r\n", (unsigned int)g_wav_volume);
    }

    return WAV_CONTROL_NONE;
}

/**
 * @brief 注册 WAV 播放使用的 I2S 句柄。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 无。
 * @note 必须在 WavPlayer_PlayFile 调用前注册，否则播放会返回 WAV_PLAYER_ERR_NO_I2S。
 */
void WavPlayer_SetI2SHandle(I2S_HandleTypeDef *hi2s)
{
    g_wav_i2s = hi2s;
}

/**
 * @brief 判断文件扩展名是否为 wav。
 * @param ext 文件扩展名字符串，不包含点号，例如 "wav"。
 * @return 1 表示是 WAV 扩展名；0 表示不是或参数为空。
 */
uint8_t WavPlayer_IsWavExtension(const char *ext)
{
    if (ext == NULL)
    {
        return 0U;
    }

    return (uint8_t)(WavPlayer_ExtCharEqual(ext[0], 'w') &&
                    WavPlayer_ExtCharEqual(ext[1], 'a') &&
                    WavPlayer_ExtCharEqual(ext[2], 'v') &&
                    ext[3] == '\0');
}

/**
 * @brief 播放一个 WAV 文件。
 * @param path FATFS 文件路径，例如 "0:/Music/test.wav"。
 * @param info_out 输出解析到的 WAV 参数；允许为 NULL。
 * @return WAV_PLAYER_OK 表示播放结束；其他值表示打开、读取、格式、WM8978 或 I2S 错误。
 * @note 该函数为阻塞式播放，播放期间仍会轮询 KEY1/KEY_UP 调节音量。
 */
WavPlayerResult WavPlayer_PlayFile(const char *path, WavInfo *info_out)
{
    FIL file;
    FRESULT fres;
    WavInfo info;
    WavPlayerResult ret;
    uint32_t remaining;
    uint8_t paused = 0U;

    if (g_wav_i2s == NULL)
    {
        return WAV_PLAYER_ERR_NO_I2S;
    }

    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        printf("[WAV] open failed path=%s res=%u\r\n", path, (unsigned int)fres);
        return WAV_PLAYER_ERR_OPEN;
    }

    ret = WavPlayer_ParseHeader(&file, &info);
    if (ret != WAV_PLAYER_OK)
    {
        (void)f_close(&file);
        printf("[WAV] parse failed code=%u\r\n", (unsigned int)ret);
        return ret;
    }

    if (info_out != NULL)
    {
        *info_out = info;
    }

    printf("[WAV] audioformat:%u channels:%u samplerate:%lu bitrate:%lu blockalign:%u bps:%u datasize:%lu datastart:%lu\r\n",
           (unsigned int)info.audio_format,
           (unsigned int)info.channels,
           (unsigned long)info.sample_rate,
           (unsigned long)(info.byte_rate * 8UL),
           (unsigned int)info.block_align,
           (unsigned int)info.bits_per_sample,
           (unsigned long)info.data_size,
           (unsigned long)info.data_offset);

    if (!WavPlayer_IsSupported(&info))
    {
        (void)f_close(&file);
        printf("[WAV] unsupported, need PCM stereo 16bit 44100Hz\r\n");
        return WAV_PLAYER_ERR_UNSUPPORTED;
    }

    (void)HAL_I2S_DMAStop(g_wav_i2s);

    /* 每次播放前重新初始化 WM8978，避免前一次 tone/MP3 测试留下音量或通道状态。 */
    if (WM8978_Init() != 0U)
    {
        (void)f_close(&file);
        return WAV_PLAYER_ERR_CODEC;
    }
    WavPlayer_ApplyVolume();
    printf("[WAV] volume=%u KEY1:- KEY_UP:+\r\n", (unsigned int)g_wav_volume);

    remaining = info.data_size;
    printf("[WAV] playback start: %s\r\n", path);

    while (remaining > 0U)
    {
        UINT br = 0U;
        uint32_t request = remaining;
        uint32_t word_count;

        if (request > WAV_READ_CHUNK_BYTES)
        {
            request = WAV_READ_CHUNK_BYTES;
        }
        request &= ~1UL;
        if (request == 0U)
        {
            break;
        }

        fres = f_read(&file, (uint8_t *)g_wav_pcm_buffer, (UINT)request, &br);
        if (fres != FR_OK)
        {
            (void)f_close(&file);
            printf("[WAV] read failed res=%u\r\n", (unsigned int)fres);
            return WAV_PLAYER_ERR_READ;
        }
        if (br == 0U)
        {
            break;
        }

        word_count = (uint32_t)br / 2UL;
        /* WAV 数据已经是 16bit stereo PCM，不需要解码，按 16bit word 直接送 I2S。 */
        if (HAL_I2S_Transmit(g_wav_i2s,
                             g_wav_pcm_buffer,
                             (uint16_t)word_count,
                             HAL_MAX_DELAY) != HAL_OK)
        {
            (void)f_close(&file);
            printf("[WAV] I2S transmit failed err=0x%08lX\r\n",
                   (unsigned long)HAL_I2S_GetError(g_wav_i2s));
            return WAV_PLAYER_ERR_I2S;
        }

        remaining -= (uint32_t)br;
        uint8_t control = WavPlayer_HandlePlaybackKey();
        if (control == WAV_CONTROL_STOP)
        {
            (void)f_close(&file);
            printf("[WAV] playback stopped\r\n");
            return WAV_PLAYER_STOPPED;
        }
        if (control == WAV_CONTROL_PAUSE)
        {
            paused = 1U;
            printf("[WAV] playback paused, KEY2:resume KEY0:stop\r\n");
        }

        while (paused != 0U)
        {
            control = WavPlayer_HandlePlaybackKey();
            if (control == WAV_CONTROL_STOP)
            {
                (void)f_close(&file);
                printf("[WAV] playback stopped\r\n");
                return WAV_PLAYER_STOPPED;
            }
            if (control == WAV_CONTROL_PAUSE)
            {
                paused = 0U;
                printf("[WAV] playback resumed\r\n");
            }
            HAL_Delay(10U);
        }
    }

    (void)f_close(&file);
    printf("[WAV] playback done\r\n");
    return WAV_PLAYER_OK;
}

/**
 * @brief 将 WAV 播放结果转换为便于串口/LCD 显示的字符串。
 * @param result WavPlayer_PlayFile 返回的结果码。
 * @return 指向静态字符串的指针。
 */
const char *WavPlayer_ResultString(WavPlayerResult result)
{
    switch (result)
    {
        case WAV_PLAYER_OK: return "OK";
        case WAV_PLAYER_ERR_NO_I2S: return "NO_I2S";
        case WAV_PLAYER_ERR_OPEN: return "OPEN";
        case WAV_PLAYER_ERR_READ: return "READ";
        case WAV_PLAYER_ERR_FORMAT: return "FORMAT";
        case WAV_PLAYER_ERR_UNSUPPORTED: return "UNSUPPORTED";
        case WAV_PLAYER_ERR_CODEC: return "CODEC";
        case WAV_PLAYER_ERR_I2S: return "I2S";
        case WAV_PLAYER_STOPPED: return "STOPPED";
        default: return "UNKNOWN";
    }
}
