#include "mp3_probe.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"

#define MP3_PROBE_SCAN_BYTES 8192U

/* MP3 探测模块只负责“确认文件像不像 MP3”，不做解码和播放。
 * 它用于早期分阶段验证：先证明 FATFS 能打开文件、能跳过 ID3、能找到帧头，
 * 再进入 minimp3 解码和 I2S 播放阶段。
 */
static uint8_t g_mp3_probe_buf[MP3_PROBE_SCAN_BYTES];

/**
 * @brief 不区分大小写比较两个扩展名字符。
 * @param a 第一个字符。
 * @param b 第二个字符。
 * @return 1 表示相等；0 表示不相等。
 */
static uint8_t Mp3Probe_ExtCharEqual(char a, char b)
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
 * @brief 以十六进制形式打印一段文件头数据。
 * @param tag 串口打印前缀，用于区分当前打印内容。
 * @param buf 待打印的数据缓冲区。
 * @param len 待打印字节数。
 * @return 无。
 */
static void Mp3Probe_PrintBytes(const char *tag, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    printf("[MP3] %s", tag);
    for (i = 0U; i < len; i++)
    {
        printf(" %02X", (unsigned int)buf[i]);
    }
    printf("\r\n");
}

/**
 * @brief 读取 ID3v2 使用的 synchsafe 32bit 长度字段。
 * @param buf 指向 4 字节 synchsafe 数据的指针。
 * @return 解码后的普通 32bit 长度。
 * @note synchsafe 每个字节只使用低 7bit，最高位不参与数值。
 */
static uint32_t Mp3Probe_ReadSynchsafe32(const uint8_t *buf)
{
    /* ID3v2 标签长度不是普通大端整数，而是每字节只使用低 7bit 的 synchsafe 格式。 */
    return ((uint32_t)(buf[0] & 0x7FU) << 21) |
           ((uint32_t)(buf[1] & 0x7FU) << 14) |
           ((uint32_t)(buf[2] & 0x7FU) << 7) |
           ((uint32_t)(buf[3] & 0x7FU));
}

/**
 * @brief 判断当前位置是否可能是合法 MP3 帧头。
 * @param buf 指向至少 4 字节数据的指针。
 * @return 1 表示符合 MP3 帧头基本规则；0 表示不符合。
 * @note 这里只做快速探测，完整解码合法性由 minimp3 再确认。
 */
static uint8_t Mp3Probe_IsFrameHeader(const uint8_t *buf)
{
    uint8_t version;
    uint8_t layer;
    uint8_t bitrate;
    uint8_t sample_rate;

    /* MP3 帧头前 11bit 为同步字，后续字段还要排除保留值和非法值。 */
    if (buf[0] != 0xFFU || ((buf[1] & 0xE0U) != 0xE0U))
    {
        return 0U;
    }

    version = (uint8_t)((buf[1] >> 3) & 0x03U);
    layer = (uint8_t)((buf[1] >> 1) & 0x03U);
    bitrate = (uint8_t)((buf[2] >> 4) & 0x0FU);
    sample_rate = (uint8_t)((buf[2] >> 2) & 0x03U);

    if (version == 0x01U || layer == 0x00U)
    {
        return 0U;
    }
    if (bitrate == 0x00U || bitrate == 0x0FU)
    {
        return 0U;
    }
    if (sample_rate == 0x03U)
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 判断文件扩展名是否为 mp3。
 * @param ext 文件扩展名字符串，不包含点号，例如 "mp3"。
 * @return 1 表示是 MP3 扩展名；0 表示不是或参数为空。
 */
uint8_t Mp3Probe_IsMp3Extension(const char *ext)
{
    if (ext == NULL)
    {
        return 0U;
    }

    return (uint8_t)(Mp3Probe_ExtCharEqual(ext[0], 'm') &&
                    Mp3Probe_ExtCharEqual(ext[1], 'p') &&
                    Mp3Probe_ExtCharEqual(ext[2], '3') &&
                    ext[3] == '\0');
}

/**
 * @brief 探测 MP3 文件基本信息。
 * @param path FATFS 文件路径，例如 "0:/Music/test.mp3"。
 * @param info_out 输出探测信息；允许为 NULL。
 * @return MP3_PROBE_OK 表示找到首个 MP3 帧；其他值表示打开、读取或帧头探测失败。
 * @note 该函数不解码、不播放，只用于确认文件可读和 MP3 帧结构存在。
 */
Mp3ProbeResult Mp3Probe_File(const char *path, Mp3ProbeInfo *info_out)
{
    FIL file;
    FRESULT fres;
    UINT br = 0U;
    Mp3ProbeInfo info;
    uint32_t scan_offset = 0U;
    uint32_t scan_len;
    uint32_t i;

    memset(&info, 0, sizeof(info));
    info.first_frame_offset = 0xFFFFFFFFUL;

    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        printf("[MP3] open failed path=%s res=%u\r\n", path, (unsigned int)fres);
        return MP3_PROBE_ERR_OPEN;
    }

    info.file_size = (uint32_t)f_size(&file);
    printf("[MP3] open: %s\r\n", path);
    printf("[MP3] size=%lu\r\n", (unsigned long)info.file_size);

    fres = f_read(&file, g_mp3_probe_buf, 16U, &br);
    if (fres != FR_OK || br == 0U)
    {
        (void)f_close(&file);
        return MP3_PROBE_ERR_READ;
    }
    Mp3Probe_PrintBytes("first bytes:", g_mp3_probe_buf, br);

    /* 如果文件前面有 ID3v2 标签，真正的音频帧通常在标签之后。 */
    if (br >= 10U && memcmp(g_mp3_probe_buf, "ID3", 3U) == 0)
    {
        uint8_t flags = g_mp3_probe_buf[5];
        uint32_t tag_size = Mp3Probe_ReadSynchsafe32(&g_mp3_probe_buf[6]);

        info.id3_skip = tag_size + 10UL;
        if ((flags & 0x10U) != 0U)
        {
            info.id3_skip += 10UL;
        }

        scan_offset = info.id3_skip;
        printf("[MP3] ID3 tag found, skip=%lu\r\n", (unsigned long)info.id3_skip);
    }
    else
    {
        printf("[MP3] ID3 tag not found\r\n");
    }

    if (f_lseek(&file, scan_offset) != FR_OK)
    {
        (void)f_close(&file);
        return MP3_PROBE_ERR_READ;
    }

    fres = f_read(&file, g_mp3_probe_buf, MP3_PROBE_SCAN_BYTES, &br);
    if (fres != FR_OK)
    {
        (void)f_close(&file);
        return MP3_PROBE_ERR_READ;
    }

    scan_len = br;
    /* 在有限窗口内查找第一个合法帧头，避免把整首歌读入 RAM。 */
    for (i = 0U; (i + 4U) <= scan_len; i++)
    {
        if (Mp3Probe_IsFrameHeader(&g_mp3_probe_buf[i]))
        {
            info.first_frame_offset = scan_offset + i;
            memcpy(info.frame_header, &g_mp3_probe_buf[i], sizeof(info.frame_header));
            break;
        }
    }

    (void)f_close(&file);

    if (info_out != NULL)
    {
        *info_out = info;
    }

    if (info.first_frame_offset == 0xFFFFFFFFUL)
    {
        printf("[MP3] first frame not found in %lu bytes from offset=%lu\r\n",
               (unsigned long)scan_len,
               (unsigned long)scan_offset);
        return MP3_PROBE_ERR_NO_FRAME;
    }

    printf("[MP3] first frame offset=%lu\r\n", (unsigned long)info.first_frame_offset);
    printf("[MP3] frame header=%02X %02X %02X %02X\r\n",
           (unsigned int)info.frame_header[0],
           (unsigned int)info.frame_header[1],
           (unsigned int)info.frame_header[2],
           (unsigned int)info.frame_header[3]);

    return MP3_PROBE_OK;
}

/**
 * @brief 将 MP3 探测结果转换为便于串口/LCD 显示的字符串。
 * @param result Mp3Probe_File 返回的结果码。
 * @return 指向静态字符串的指针。
 */
const char *Mp3Probe_ResultString(Mp3ProbeResult result)
{
    switch (result)
    {
        case MP3_PROBE_OK: return "OK";
        case MP3_PROBE_ERR_OPEN: return "OPEN";
        case MP3_PROBE_ERR_READ: return "READ";
        case MP3_PROBE_ERR_NO_FRAME: return "NO_FRAME";
        default: return "UNKNOWN";
    }
}
