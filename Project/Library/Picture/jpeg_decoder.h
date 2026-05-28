#ifndef JPEG_DECODER_H
#define JPEG_DECODER_H

#include <stdint.h>

/**
 * @brief JPEG 解码与显示过程的执行结果
 * @details
 * 该枚举只描述 JPEG 运行时解码链路的状态，
 * 上层图片模块会再把它映射成统一的 `PictureResult`。
 */
typedef enum
{
    /** JPEG 文件已成功解码并显示到 LCD。 */
    JPEG_DECODER_OK = 0,
    /** 文件打开失败，例如路径错误或 SD 未挂载。 */
    JPEG_DECODER_ERR_OPEN,
    /** JPEG 文件结构损坏或头信息非法。 */
    JPEG_DECODER_ERR_FORMAT,
    /** 文件读取失败。 */
    JPEG_DECODER_ERR_READ,
    /** JPEG 标准超出当前 TJpgDec 支持范围。 */
    JPEG_DECODER_ERR_UNSUPPORTED,
    /** 显示区域参数非法或超出 LCD 有效范围。 */
    JPEG_DECODER_ERR_RANGE,
    /** 解码工作区内存不足。 */
    JPEG_DECODER_ERR_NOMEM
} JpegDecoderResult;

/**
 * @brief 将 JPEG 解码状态转换为短文本
 * @param result JPEG 解码状态
 * @return 可直接用于状态栏显示的短文本
 */
const char *JpegDecoder_ResultString(JpegDecoderResult result);

/**
 * @brief 从 SD 卡读取 JPEG 文件并显示到 LCD
 * @param path JPEG 文件路径，例如 `0:/Picture/demo.jpg`
 * @param x 预览区域左上角 X 坐标
 * @param y 预览区域左上角 Y 坐标
 * @param width 预览区域宽度
 * @param height 预览区域高度
 * @return JpegDecoderResult 执行结果
 * @details
 * 当前第一版 JPEG 路径的策略如下：
 * 1. 使用 `TJpgDec` 做软件解码；
 * 2. 输入源来自 `FATFS` 文件；
 * 3. 输出目标像素格式为 `RGB565`；
 * 4. 优先利用 `TJpgDec` 自带的离散缩放级别；
 * 5. 缩放后的结果在预览区域内居中显示；
 * 6. 当前主要保证 `Baseline JPEG`，不保证渐进式 JPEG 可用。
 */
JpegDecoderResult JpegDecoder_ShowFile(const char *path, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

#endif

