#include "jpeg_decoder.h"

#include "ff.h"
#include "tjpgd.h"
#include "../tftlcd/tftlcd.h"

/**
 * @file jpeg_decoder.c
 * @brief 基于 TJpgDec 的 JPEG 运行时解码与 LCD 显示封装
 * @details
 * 当前模块负责把 FATFS、TJpgDec 和 LCD 输出连接起来。
 * 第一版缩放策略如下：
 * 1. 优先使用 TJpgDec 自带的 1/1、1/2、1/4、1/8 缩放级别；
 * 2. 只对大图做缩小，小图保持原尺寸；
 * 3. 缩放后在预览区域内居中显示；
 * 4. 不做额外的软件插值放大。
 */

/** JPEG 解码工作区大小，包含 TJpgDec 输入缓冲和 MCU 工作区。 */
#define JPEG_WORK_POOL_SIZE (16U * 1024U)

/**
 * @brief 单次 JPEG 显示会话的上下文
 * @details
 * TJpgDec 的输入/输出回调都只能拿到 JDEC 指针，
 * 因此通过 device 字段传递当前文件和目标显示参数。
 */
typedef struct
{
    /** 已打开的 JPEG 文件对象。 */
    FIL *file;
    /** 实际绘制起点 X 坐标。 */
    uint16_t draw_x;
    /** 实际绘制起点 Y 坐标。 */
    uint16_t draw_y;
} JpegDecoderContext;

/** 使用静态工作区，避免 MCU 端引入动态内存分配。 */
static uint8_t g_jpeg_work_pool[JPEG_WORK_POOL_SIZE];

/**
 * @brief 计算指定 TJpgDec 缩放级别后的输出尺寸
 * @param src 源尺寸
 * @param scale TJpgDec 缩放级别，0~3 分别表示 1/1、1/2、1/4、1/8
 * @return uint16_t 缩放后的尺寸
 * @details
 * TJpgDec 的缩放本质是按 2 的幂做降采样。
 * 为了避免向下截断后出现 0，这里使用向上取整。
 */
static uint16_t JpegDecoder_ScaledSize(uint16_t src, uint8_t scale)
{
    return (uint16_t)((src + ((1U << scale) - 1U)) >> scale);
}

/**
 * @brief 为 JPEG 选择最合适的 TJpgDec 缩放级别
 * @param src_width 源图宽度
 * @param src_height 源图高度
 * @param max_width 预览区域宽度
 * @param max_height 预览区域高度
 * @param out_width 输出显示宽度
 * @param out_height 输出显示高度
 * @return uint8_t 选择出的 TJpgDec 缩放级别
 * @details
 * 选择原则：
 * 1. 如果原图已经能放进预览区域，则不缩放；
 * 2. 否则选择“第一个能完整放入预览区域”的缩放级别；
 * 3. 如果 1/8 仍然放不下，则退化为 1/8，并由 LCD 再做边界裁剪。
 */
static uint8_t JpegDecoder_SelectScale(uint16_t src_width,
                                       uint16_t src_height,
                                       uint16_t max_width,
                                       uint16_t max_height,
                                       uint16_t *out_width,
                                       uint16_t *out_height)
{
    uint8_t scale;

    if (src_width <= max_width && src_height <= max_height)
    {
        *out_width = src_width;
        *out_height = src_height;
        return 0U;
    }

    for (scale = 1U; scale <= 3U; scale++)
    {
        uint16_t scaled_width = JpegDecoder_ScaledSize(src_width, scale);
        uint16_t scaled_height = JpegDecoder_ScaledSize(src_height, scale);

        if (scaled_width <= max_width && scaled_height <= max_height)
        {
            *out_width = scaled_width;
            *out_height = scaled_height;
            return scale;
        }
    }

    *out_width = JpegDecoder_ScaledSize(src_width, 3U);
    *out_height = JpegDecoder_ScaledSize(src_height, 3U);
    return 3U;
}

/**
 * @brief FATFS 输入回调，适配 TJpgDec 的输入接口
 * @param jd TJpgDec 解码对象
 * @param buf 输出缓冲区；为 NULL 时表示仅跳过数据
 * @param len 请求读取或跳过的字节数
 * @return size_t 实际处理的字节数
 */
static size_t JpegDecoder_Input(JDEC *jd, uint8_t *buf, size_t len)
{
    JpegDecoderContext *ctx;
    UINT actual = 0U;
    FRESULT fres;

    if (jd == NULL || jd->device == NULL)
    {
        return 0U;
    }

    ctx = (JpegDecoderContext *)jd->device;
    if (ctx->file == NULL)
    {
        return 0U;
    }

    if (buf != NULL)
    {
        fres = f_read(ctx->file, buf, (UINT)len, &actual);
        if (fres != FR_OK)
        {
            return 0U;
        }
        return (size_t)actual;
    }

    fres = f_lseek(ctx->file, f_tell(ctx->file) + (FSIZE_t)len);
    if (fres != FR_OK)
    {
        return 0U;
    }
    return len;
}

/**
 * @brief 将 TJpgDec 输出的 RGB565 像素块写入 LCD
 * @param jd TJpgDec 解码对象
 * @param bitmap TJpgDec 输出的 RGB565 像素块
 * @param rect 当前像素块在“缩放后图像坐标系”中的矩形位置
 * @return int 1=继续解码，0=中断
 * @details
 * 当前 JPEG 缩放已经在 TJpgDec 内部完成，所以这里不再做额外缩放，
 * 只负责：
 * 1. 把缩放后的图像整体平移到居中坐标；
 * 2. 对超出 LCD 边界的少量像素做裁剪。
 */
static int JpegDecoder_Output(JDEC *jd, void *bitmap, JRECT *rect)
{
    JpegDecoderContext *ctx;
    uint16_t *pixels;
    uint16_t block_width;
    uint16_t block_height;
    uint16_t draw_width;
    uint16_t draw_height;
    uint16_t target_x;
    uint16_t target_y;
    uint16_t row;
    uint16_t col;

    if (jd == NULL || jd->device == NULL || bitmap == NULL || rect == NULL)
    {
        return 0;
    }

    ctx = (JpegDecoderContext *)jd->device;
    pixels = (uint16_t *)bitmap;

    target_x = (uint16_t)(ctx->draw_x + rect->left);
    target_y = (uint16_t)(ctx->draw_y + rect->top);
    block_width = (uint16_t)(rect->right - rect->left + 1U);
    block_height = (uint16_t)(rect->bottom - rect->top + 1U);

    if (target_x >= tftlcd_data.width || target_y >= tftlcd_data.height)
    {
        return 1;
    }

    draw_width = (uint16_t)(((uint32_t)(tftlcd_data.width - target_x) < block_width)
                                 ? (tftlcd_data.width - target_x)
                                 : block_width);
    draw_height = (uint16_t)(((uint32_t)(tftlcd_data.height - target_y) < block_height)
                                  ? (tftlcd_data.height - target_y)
                                  : block_height);

    if (draw_width == 0U || draw_height == 0U)
    {
        return 1;
    }

    for (row = 0; row < draw_height; row++)
    {
        const uint16_t *row_pixels = &pixels[row * block_width];

        LCD_Set_Window(target_x,
                       (uint16_t)(target_y + row),
                       (uint16_t)(target_x + draw_width - 1U),
                       (uint16_t)(target_y + row));
        for (col = 0; col < draw_width; col++)
        {
            LCD_WriteData_Color(row_pixels[col]);
        }
    }

    return 1;
}

/**
 * @brief 把 TJpgDec 返回码映射为工程内部状态码
 * @param result TJpgDec 返回码
 * @return JpegDecoderResult 工程内部状态码
 */
static JpegDecoderResult JpegDecoder_MapResult(JRESULT result)
{
    switch (result)
    {
        case JDR_OK:
            return JPEG_DECODER_OK;
        case JDR_MEM1:
        case JDR_MEM2:
            return JPEG_DECODER_ERR_NOMEM;
        case JDR_FMT2:
        case JDR_FMT3:
            return JPEG_DECODER_ERR_UNSUPPORTED;
        case JDR_FMT1:
            return JPEG_DECODER_ERR_FORMAT;
        case JDR_INTR:
        case JDR_INP:
        case JDR_PAR:
        default:
            return JPEG_DECODER_ERR_READ;
    }
}

JpegDecoderResult JpegDecoder_ShowFile(const char *path,
                                       uint16_t x,
                                       uint16_t y,
                                       uint16_t width,
                                       uint16_t height)
{
    FIL file;
    JDEC decoder;
    JRESULT result;
    JpegDecoderContext context;
    FRESULT fres;
    uint16_t box_width;
    uint16_t box_height;
    uint16_t draw_width;
    uint16_t draw_height;
    uint8_t scale;

    if (path == NULL)
    {
        return JPEG_DECODER_ERR_OPEN;
    }
    if (x >= tftlcd_data.width || y >= tftlcd_data.height || width == 0U || height == 0U)
    {
        return JPEG_DECODER_ERR_RANGE;
    }

    box_width = (uint16_t)(((uint32_t)(tftlcd_data.width - x) < width) ? (tftlcd_data.width - x) : width);
    box_height = (uint16_t)(((uint32_t)(tftlcd_data.height - y) < height) ? (tftlcd_data.height - y) : height);
    if (box_width == 0U || box_height == 0U)
    {
        return JPEG_DECODER_ERR_RANGE;
    }

    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        return JPEG_DECODER_ERR_OPEN;
    }

    context.file = &file;
    context.draw_x = x;
    context.draw_y = y;

    /* 先解析 JPEG 头部，拿到原始宽高后再决定使用哪个缩放级别。 */
    result = jd_prepare(&decoder, JpegDecoder_Input,
                        g_jpeg_work_pool, sizeof(g_jpeg_work_pool),
                        &context);
    if (result != JDR_OK)
    {
        (void)f_close(&file);
        return JpegDecoder_MapResult(result);
    }

    scale = JpegDecoder_SelectScale(decoder.width, decoder.height,
                                    box_width, box_height,
                                    &draw_width, &draw_height);
    context.draw_x = (uint16_t)(x + (uint16_t)((box_width - draw_width) / 2U));
    context.draw_y = (uint16_t)(y + (uint16_t)((box_height - draw_height) / 2U));

    result = jd_decomp(&decoder, JpegDecoder_Output, scale);
    (void)f_close(&file);
    return JpegDecoder_MapResult(result);
}

const char *JpegDecoder_ResultString(JpegDecoderResult result)
{
    switch (result)
    {
        case JPEG_DECODER_OK:
            return "JPG OK";
        case JPEG_DECODER_ERR_OPEN:
            return "JPG OPEN";
        case JPEG_DECODER_ERR_FORMAT:
            return "JPG FORMAT";
        case JPEG_DECODER_ERR_READ:
            return "JPG READ";
        case JPEG_DECODER_ERR_UNSUPPORTED:
            return "JPG UNSUP";
        case JPEG_DECODER_ERR_RANGE:
            return "JPG RANGE";
        case JPEG_DECODER_ERR_NOMEM:
            return "JPG NOMEM";
        default:
            return "JPG ERR";
    }
}
