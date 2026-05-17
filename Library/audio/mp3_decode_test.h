#ifndef MP3_DECODE_TEST_H
#define MP3_DECODE_TEST_H

#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MP3_DECODE_TEST_OK = 0,
    MP3_DECODE_TEST_ERR_OPEN,
    MP3_DECODE_TEST_ERR_READ,
    MP3_DECODE_TEST_ERR_NO_FRAME,
    MP3_DECODE_TEST_ERR_DECODE,
    MP3_DECODE_TEST_ERR_NO_I2S,
    MP3_DECODE_TEST_ERR_CODEC,
    MP3_DECODE_TEST_ERR_UNSUPPORTED,
    MP3_DECODE_TEST_ERR_I2S,
    MP3_DECODE_TEST_STOPPED
} Mp3DecodeTestResult;

/**
 * @brief 注册 MP3 播放使用的 I2S 句柄。
 * @param hi2s I2S 句柄，当前工程传入 &hi2s2。
 * @return 无。
 */
void Mp3DecodeTest_SetI2SHandle(I2S_HandleTypeDef *hi2s);

/**
 * @brief 播放指定 MP3 文件。
 * @param path FATFS 文件路径。
 * @return MP3_DECODE_TEST_OK 表示播放完成；其他值表示失败原因。
 */
Mp3DecodeTestResult Mp3DecodeTest_File(const char *path);

/**
 * @brief 将 MP3 播放结果码转换为字符串。
 * @param result 播放结果码。
 * @return 结果字符串。
 */
const char *Mp3DecodeTest_ResultString(Mp3DecodeTestResult result);

#ifdef __cplusplus
}
#endif

#endif
