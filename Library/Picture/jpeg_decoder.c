#include "jpeg_decoder.h"

#include "ff.h"
#include "tjpgd.h"
#include "../tftlcd/tftlcd.h"

/**
 * @file jpeg_decoder.c
 * @brief 基于 TJpgDec 的 JPEG 运行时解码与 LCD 显示封装
 * @details
 * 当前模块的职责是把 FATFS 文件读取、TJpgDec 解码和 LCD 输出桥接起来。
 * 整体数据流如下：
 * 1. FATFS 从 SD 卡读取 JPEG 压缩数据；
 * 2. TJpgDec 解析 JPEG 段并输出 RGB565 像素块；
 * 3. 输出回调按矩形块逐行写入 LCD；
 * 4. 超出屏幕的像素仅裁剪，不额外缩放。
 */

/** JPEG 解码工作区大小，包含 TJpgDec 所需的输入缓冲和 MCU 工作区。 */
#define JPEG_WORK_POOL_SIZE (16U * 1024U)

/**
 * @brief 单次 JPEG 显示会话上下文
 * @details
 * TJpgDec 的输入回调和输出回调都只能拿到 JDEC 指针，
 * 因此需要通过 device 字段把当前文件和 LCD 目标位置传进去。
 */
typedef struct
{
    /** 已打开的 JPEG 文件对象。 */
    FIL *file;
    /** 图片左上角目标 X 坐标。 */
    uint16_t lcd_x;
    /** 图片左上角目标 Y 坐标。 */
    uint16_t lcd_y;
} JpegDecoderContext;

/** JPEG 解码工作区使用静态缓冲，避免在 MCU 上引入动态内存分配。 */
static uint8_t g_jpeg_work_pool[JPEG_WORK_POOL_SIZE];

/**
 * @brief 把 FATFS 读文件接口适配为 TJpgDec 输入回调
 * @param jd TJpgDec 解码对象
 * @param buf 输出缓冲区；为 NULL 时表示仅跳过数据
 * @param len 请求读取或跳过的字节数
 * @return size_t 实际处理的字节数
 * @details
 * TJpgDec 约定：
 * 1. buf != NULL 时，回调需要把数据读入 buf；
 * 2. buf == NULL 时，回调只需要跳过 len 字节。
 * 这里通过 FATFS 的 f_read / f_lseek 实现这两种行为。
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
 * @brief 将 TJpgDec 输出的 RGB565 矩形块裁剪后写入 LCD
 * @param jd TJpgDec 解码对象
 * @param bitmap TJpgDec 输出的像素块首地址，当前配置下为 RGB565
 * @param rect 当前像素块在源图中的矩形位置
 * @return int 1 表示继续解码，0 表示中断
 * @details
 * TJpgDec 会按 MCU 块回调输出，不一定是一整行或整图。
 * 这里的处理步骤是：
 * 1. 计算当前块映射到 LCD 后的目标坐标；
 * 2. 如果完全在屏幕外，则跳过该块；
 * 3. 如果部分超出，则只保留可见宽高；
 * 4. 逐行设置 LCD 窗口并输出 RGB565 像素。
 */
static int JpegDecoder_Output(JDEC *jd, void *bitmap, JRECT *rect)
{
    JpegDecoderContext *ctx;
    uint16_t *pixels;
    uint16_t block_width;
    uint16_t block_height;
    uint16_t draw_width;
    uint16_t draw_height;
    uint16_t row;
    uint16_t col;
    uint16_t target_x;
    uint16_t target_y;

    if (jd == NULL || jd->device == NULL || bitmap == NULL || rect == NULL)
    {
        return 0;
    }

    ctx = (JpegDecoderContext *)jd->device;
    pixels = (uint16_t *)bitmap;

    target_x = (uint16_t)(ctx->lcd_x + rect->left);
    target_y = (uint16_t)(ctx->lcd_y + rect->top);
    block_width = (uint16_t)(rect->right - rect->left + 1U);
    block_height = (uint16_t)(rect->bottom - rect->top + 1U);

    /* 当前块如果完全落在屏幕外，直接跳过并继续解码后续块。 */
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

        LCD_Set_Window(target_x, (uint16_t)(target_y + row),
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
 * @brief 将 TJpgDec 的返回码映射为模块内部状态码
 * @param result TJpgDec 返回码
 * @return JpegDecoderResult 映射后的 JPEG 显示结果
 * @details
 * 这里把第三方库的错误语义压缩成工程内部更容易理解的状态集合，
 * 便于上层 UI 统一处理。
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

JpegDecoderResult JpegDecoder_ShowFile(const char *path, uint16_t x, uint16_t y)
{
    FIL file;
    JDEC decoder;
    JRESULT result;
    JpegDecoderContext context;
    FRESULT fres;

    /* 坐标合法性先由本模块兜底，避免进入解码后再发现输出越界。 */
    if (path == NULL)
    {
        return JPEG_DECODER_ERR_OPEN;
    }
    if (x >= tftlcd_data.width || y >= tftlcd_data.height)
    {
        return JPEG_DECODER_ERR_RANGE;
    }

    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        return JPEG_DECODER_ERR_OPEN;
    }

    context.file = &file;
    context.lcd_x = x;
    context.lcd_y = y;

    /* 先解析 JPEG 结构、量化表和 Huffman 表，建立解码会话。 */
    result = jd_prepare(&decoder, JpegDecoder_Input,
                        g_jpeg_work_pool, sizeof(g_jpeg_work_pool),
                        &context);
    if (result != JDR_OK)
    {
        (void)f_close(&file);
        return JpegDecoder_MapResult(result);
    }

    /* 当前第一版不做缩放，scale=0 表示按原始像素尺寸输出。 */
    result = jd_decomp(&decoder, JpegDecoder_Output, 0U);
    (void)f_close(&file);
    return JpegDecoder_MapResult(result);
}
