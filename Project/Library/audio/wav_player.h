#ifndef WAV_PLAYER_H
#define WAV_PLAYER_H

#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WAV_PLAYER_OK = 0,
    WAV_PLAYER_ERR_NO_I2S,
    WAV_PLAYER_ERR_OPEN,
    WAV_PLAYER_ERR_READ,
    WAV_PLAYER_ERR_FORMAT,
    WAV_PLAYER_ERR_UNSUPPORTED,
    WAV_PLAYER_ERR_CODEC,
    WAV_PLAYER_ERR_I2S,
    WAV_PLAYER_STOPPED
} WavPlayerResult;

typedef struct
{
    /* RIFF/WAVE fmt 和 data chunk 中解析出的关键播放参数。 */
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} WavInfo;

/**
 * @brief 注册 WAV 播放使用的 I2S 句柄。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 无。
 */
void WavPlayer_SetI2SHandle(I2S_HandleTypeDef *hi2s);

/**
 * @brief 判断扩展名是否为 WAV。
 * @param ext 文件扩展名，不包含点号。
 * @return 1 表示 WAV；0 表示不是 WAV 或参数为空。
 */
uint8_t WavPlayer_IsWavExtension(const char *ext);

/**
 * @brief 播放指定 WAV 文件。
 * @param path FATFS 文件路径。
 * @param info_out 输出 WAV 文件格式信息，允许为 NULL。
 * @return WAV_PLAYER_OK 表示播放完成；其他值表示失败原因。
 */
WavPlayerResult WavPlayer_PlayFile(const char *path, WavInfo *info_out);

/**
 * @brief 将 WAV 播放结果码转换为字符串。
 * @param result 播放结果码。
 * @return 结果字符串。
 */
const char *WavPlayer_ResultString(WavPlayerResult result);

#ifdef __cplusplus
}
#endif

#endif
