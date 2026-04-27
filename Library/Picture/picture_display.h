#ifndef PICTURE_DISPLAY_H
#define PICTURE_DISPLAY_H

#include <stdint.h>

/**
 * @brief 图片显示模块执行结果
 * @details
 * 该枚举用于向上层 UI 返回图片显示过程中的执行状态。
 * UI 层拿到状态码后，可以直接在 LCD 底部显示对应提示，
 * 也可以根据状态码决定是否继续后续处理。
 */
typedef enum
{
    /** 执行成功，图片已经显示到 LCD 指定区域。 */
    PICTURE_OK = 0,
    /** 文件打开失败，常见原因是路径错误、文件不存在或 SD 未挂载。 */
    PICTURE_ERR_OPEN,
    /** 文件格式错误，例如不是标准 BMP 文件或头信息损坏。 */
    PICTURE_ERR_FORMAT,
    /** 文件读取失败，例如 f_read/f_lseek 返回错误。 */
    PICTURE_ERR_READ,
    /** 当前图片格式超出第一版驱动支持范围。 */
    PICTURE_ERR_UNSUPPORTED,
    /** 显示坐标超出 LCD 可显示范围。 */
    PICTURE_ERR_RANGE
} PictureResult;

/**
 * @brief 根据文件扩展名自动选择图片显示路径
 * @param path 图片文件路径，例如 "0:/Picture/demo.bmp" 或 "0:/Picture/demo.jpg"
 * @param x 图片左上角显示起始 X 坐标
 * @param y 图片左上角显示起始 Y 坐标
 * @return PictureResult 显示结果状态码
 * @details
 * 该接口是上层 UI 推荐使用的统一入口。
 * 当前版本会根据扩展名分发到：
 * 1. BMP -> Picture_ShowBMP()
 * 2. JPG/JPEG -> Picture_ShowJPG()
 */
PictureResult Picture_Show(const char *path, uint16_t x, uint16_t y);

/**
 * @brief 从 SD 卡路径读取 BMP 文件并显示到 LCD
 * @param path BMP 文件路径，例如 "0:/test.bmp"
 * @param x 图片左上角显示起始 X 坐标
 * @param y 图片左上角显示起始 Y 坐标
 * @return PictureResult 显示结果状态码
 * @details
 * 第一版实现重点是先打通“文件读取 -> BMP 解析 -> RGB565 转换 -> LCD 显示”的完整链路。
 * 当前实现特点如下：
 * 1. 优先支持标准 BMP 文件；
 * 2. 使用逐行读取方式，避免整图加载占用过多 RAM；
 * 3. 输出目标像素格式为 LCD 常用的 RGB565；
 * 4. 图片超出屏幕时仅显示可见区域，不做缩放；
 * 5. 发生错误时返回明确状态码，便于 UI 层提示。
 */
PictureResult Picture_ShowBMP(const char *path, uint16_t x, uint16_t y);

/**
 * @brief 从 SD 卡路径读取 JPG/JPEG 文件并显示到 LCD
 * @param path JPG/JPEG 文件路径，例如 "0:/Picture/demo.jpg"
 * @param x 图片左上角显示起始 X 坐标
 * @param y 图片左上角显示起始 Y 坐标
 * @return PictureResult 显示结果状态码
 * @details
 * 当前实现内部基于 TJpgDec 做软件解码：
 * 1. 只保证基线 JPEG；
 * 2. 解码结果直接输出为 RGB565；
 * 3. 图片超出屏幕时只显示可见区域；
 * 4. 当前版本不做缩放。
 */
PictureResult Picture_ShowJPG(const char *path, uint16_t x, uint16_t y);

/**
 * @brief 将图片显示状态码转换为短文本
 * @param result 图片显示结果状态码
 * @return const char* 可直接显示在 UI 状态栏中的文本
 * @details
 * 该函数主要用于调试和 UI 状态提示，避免上层重复编写状态码到字符串的映射逻辑。
 */
const char *Picture_ResultString(PictureResult result);

#endif
