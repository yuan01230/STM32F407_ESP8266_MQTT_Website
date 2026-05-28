#ifndef MP3_PROBE_H
#define MP3_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MP3_PROBE_OK = 0,
    MP3_PROBE_ERR_OPEN,
    MP3_PROBE_ERR_READ,
    MP3_PROBE_ERR_NO_FRAME
} Mp3ProbeResult;

typedef struct
{
    uint32_t file_size;
    /* ID3v2 标签长度。真正的 MP3 帧通常从该偏移之后开始。 */
    uint32_t id3_skip;
    uint32_t first_frame_offset;
    uint8_t frame_header[4];
} Mp3ProbeInfo;

/**
 * @brief 判断扩展名是否为 MP3。
 * @param ext 文件扩展名，不包含点号。
 * @return 1 表示 MP3；0 表示不是 MP3 或参数为空。
 */
uint8_t Mp3Probe_IsMp3Extension(const char *ext);

/**
 * @brief 打开文件并探测 ID3 标签和首个 MP3 帧头。
 * @param path FATFS 文件路径。
 * @param info_out 输出探测信息，允许为 NULL。
 * @return MP3_PROBE_OK 表示探测成功；其他值表示打开、读取或未找到帧。
 */
Mp3ProbeResult Mp3Probe_File(const char *path, Mp3ProbeInfo *info_out);

/**
 * @brief 将探测结果码转换为字符串。
 * @param result 探测结果码。
 * @return 结果字符串。
 */
const char *Mp3Probe_ResultString(Mp3ProbeResult result);

#ifdef __cplusplus
}
#endif

#endif
