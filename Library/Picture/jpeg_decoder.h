#ifndef JPEG_DECODER_H
#define JPEG_DECODER_H

#include <stdint.h>

/**
 * @brief JPEG 解码并显示过程的执行结果
 * @details
 * 该枚举只描述 JPEG 运行时解码链路的状态，
 * 由上层图片模块再映射到统一的 PictureResult。
 */
typedef enum
{
    /** JPEG 文件已成功解码并显示到 LCD。 */
    JPEG_DECODER_OK = 0,
    /** 打开文件失败，常见原因是路径错误或 SD 未挂载。 */
    JPEG_DECODER_ERR_OPEN,
    /** JPEG 文件格式损坏或头信息非法。 */
    JPEG_DECODER_ERR_FORMAT,
    /** 文件读取过程失败。 */
    JPEG_DECODER_ERR_READ,
    /** JPEG 标准超出当前 TJpgDec 支持范围，例如渐进式 JPEG。 */
    JPEG_DECODER_ERR_UNSUPPORTED,
    /** 显示坐标超出 LCD 有效范围。 */
    JPEG_DECODER_ERR_RANGE,
    /** 解码过程中工作区内存不足。 */
    JPEG_DECODER_ERR_NOMEM
} JpegDecoderResult;

/**
 * @brief 从 SD 卡读取 JPEG 文件并直接显示到 LCD
 * @param path JPEG 文件路径，例如 "0:/Picture/demo.jpg"
 * @param x 图片左上角目标 X 坐标
 * @param y 图片左上角目标 Y 坐标
 * @return JpegDecoderResult 执行结果
 * @details
 * 当前第一版 JPEG 路径的策略如下：
 * 1. 使用 TJpgDec 做软件解码；
 * 2. 输入源来自 FATFS 文件；
 * 3. 输出格式固定为 RGB565，便于直接写 LCD；
 * 4. 图片超出屏幕时仅做裁剪，不做缩放；
 * 5. 当前主要支持基线 JPEG，不保证渐进式 JPEG 可用。
 */
JpegDecoderResult JpegDecoder_ShowFile(const char *path, uint16_t x, uint16_t y);

#endif
