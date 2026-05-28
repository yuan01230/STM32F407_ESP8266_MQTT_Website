#include "picture_display.h"

#include "ff.h"
#include "jpeg_decoder.h"
#include "../tftlcd/tftlcd.h"

/*
 * 说明：
 * - 本文件是图片预览统一入口。
 * - UI 层只需要调用 Picture_Show()，不需要自己区分 BMP 还是 JPG。
 * - 模块内部会负责：
 *   1. 识别扩展名；
 *   2. 计算预览区域中的缩放与居中布局；
 *   3. 调用具体格式的解码与显示路径；
 *   4. 返回统一错误码给上层 UI。
 */

/**
 * @file picture_display.c
 * @brief 图片显示模块，负责 BMP/JPG 的统一预览入口
 * @details
 * 当前版本的核心目标：
 * 1. 统一 BMP 和 JPG/JPEG 的预览接口；
 * 2. 让大图按原比例缩小后完整进入预览区域；
 * 3. 让小图保持原尺寸并在预览区域中居中显示；
 * 4. 在 MCU 内存受限的前提下，优先采用逐行处理和轻量缩放策略。
 */

/** BMP 文件头固定长度。 */
#define BMP_FILE_HEADER_SIZE 14U
/** 常见 BITMAPINFOHEADER 长度。 */
#define BMP_INFO_HEADER_SIZE 40U
/** BMP 无压缩标志。 */
#define BMP_COMPRESSION_RGB 0U
/** BMP 位域压缩标志，当前仅做简单兼容。 */
#define BMP_COMPRESSION_BITFIELDS 3U
/** LCD 当前宽度为 320，因此目标行缓冲按 320 像素分配。 */
#define PICTURE_MAX_DEST_WIDTH 320U
/** 源 BMP 单行最大缓存字节数，用于支持比 LCD 更宽的 BMP 源图。 */
#define BMP_MAX_ROW_BUFFER_BYTES 4096U

/**
 * @brief 图片在预览区域中的最终布局结果
 */
typedef struct
{
    /** 实际绘制起始 X 坐标。 */
    uint16_t draw_x;
    /** 实际绘制起始 Y 坐标。 */
    uint16_t draw_y;
    /** 实际绘制宽度。 */
    uint16_t draw_width;
    /** 实际绘制高度。 */
    uint16_t draw_height;
    /** 裁剪后的有效预览区域宽度。 */
    uint16_t box_width;
    /** 裁剪后的有效预览区域高度。 */
    uint16_t box_height;
} PictureLayout;

/**
 * @brief 读取小端格式的 16 位整数
 */
static uint16_t Picture_ReadLe16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * @brief 读取小端格式的 32 位整数
 */
static uint32_t Picture_ReadLe32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

/**
 * @brief 读取带符号的小端 32 位整数
 */
static int32_t Picture_ReadLe32Signed(const uint8_t *buf)
{
    return (int32_t)Picture_ReadLe32(buf);
}

/**
 * @brief 将 24 位 BGR 像素转换为 RGB565
 */
static uint16_t Picture_Bgr888ToRgb565(const uint8_t *pixel)
{
    uint16_t red = (uint16_t)(pixel[2] >> 3);
    uint16_t green = (uint16_t)(pixel[1] >> 2);
    uint16_t blue = (uint16_t)(pixel[0] >> 3);
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

/**
 * @brief 将简单 16 位 BMP 像素转换为 RGB565
 */
static uint16_t Picture_Bgr555ToRgb565(uint16_t pixel)
{
    uint16_t red5 = (uint16_t)((pixel >> 10) & 0x1FU);
    uint16_t green5 = (uint16_t)((pixel >> 5) & 0x1FU);
    uint16_t blue5 = (uint16_t)(pixel & 0x1FU);
    uint16_t green6 = (uint16_t)((green5 << 1) | (green5 >> 4));
    return (uint16_t)((red5 << 11) | (green6 << 5) | blue5);
}

/**
 * @brief 忽略 ASCII 大小写比较两个扩展名
 */
static uint8_t Picture_ExtEquals(const char *lhs, const char *rhs)
{
    uint16_t index = 0U;

    if (lhs == NULL || rhs == NULL)
    {
        return 0U;
    }

    while (lhs[index] != '\0' && rhs[index] != '\0')
    {
        char a = lhs[index];
        char b = rhs[index];

        if (a >= 'A' && a <= 'Z')
        {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b)
        {
            return 0U;
        }
        index++;
    }

    return (uint8_t)(lhs[index] == '\0' && rhs[index] == '\0');
}

/**
 * @brief 从完整路径中查找扩展名
 */
static const char *Picture_FindExtension(const char *path)
{
    const char *cursor;
    const char *dot = NULL;

    if (path == NULL)
    {
        return NULL;
    }

    for (cursor = path; *cursor != '\0'; cursor++)
    {
        if (*cursor == '.')
        {
            dot = cursor + 1;
        }
    }

    if (dot == NULL || *dot == '\0')
    {
        return NULL;
    }

    return dot;
}

/**
 * @brief 计算图片在指定预览区域中的等比布局
 * @param src_width 源图宽度
 * @param src_height 源图高度
 * @param box_x 预览区域左上角 X 坐标
 * @param box_y 预览区域左上角 Y 坐标
 * @param box_width 预览区域宽度
 * @param box_height 预览区域高度
 * @param out 输出布局结果
 * @return uint8_t 1=成功，0=预览区域无效
 * @details
 * 规则：
 * 1. 大图按原比例缩小，完整装入预览区域；
 * 2. 小图不放大，保持原尺寸；
 * 3. 结果在预览区域中居中。
 */
static uint8_t Picture_ComputeLayout(uint32_t src_width,
                                     uint32_t src_height,
                                     uint16_t box_x,
                                     uint16_t box_y,
                                     uint16_t box_width,
                                     uint16_t box_height,
                                     PictureLayout *out)
{
    uint16_t clipped_width;
    uint16_t clipped_height;
    uint32_t draw_width;
    uint32_t draw_height;

    if (out == NULL || src_width == 0U || src_height == 0U)
    {
        return 0U;
    }
    if (box_x >= tftlcd_data.width || box_y >= tftlcd_data.height || box_width == 0U || box_height == 0U)
    {
        return 0U;
    }

    clipped_width = (uint16_t)(((uint32_t)(tftlcd_data.width - box_x) < box_width)
                                    ? (tftlcd_data.width - box_x)
                                    : box_width);
    clipped_height = (uint16_t)(((uint32_t)(tftlcd_data.height - box_y) < box_height)
                                     ? (tftlcd_data.height - box_y)
                                     : box_height);
    if (clipped_width == 0U || clipped_height == 0U)
    {
        return 0U;
    }

    if (src_width <= clipped_width && src_height <= clipped_height)
    {
        draw_width = src_width;
        draw_height = src_height;
    }
    else if (src_width * clipped_height >= src_height * clipped_width)
    {
        draw_width = clipped_width;
        draw_height = (src_height * clipped_width) / src_width;
    }
    else
    {
        draw_height = clipped_height;
        draw_width = (src_width * clipped_height) / src_height;
    }

    if (draw_width == 0U)
    {
        draw_width = 1U;
    }
    if (draw_height == 0U)
    {
        draw_height = 1U;
    }

    out->box_width = clipped_width;
    out->box_height = clipped_height;
    out->draw_width = (uint16_t)draw_width;
    out->draw_height = (uint16_t)draw_height;
    out->draw_x = (uint16_t)(box_x + (uint16_t)((clipped_width - out->draw_width) / 2U));
    out->draw_y = (uint16_t)(box_y + (uint16_t)((clipped_height - out->draw_height) / 2U));
    return 1U;
}

/**
 * @brief 把一行 RGB565 像素写入 LCD
 */
static void Picture_DrawRow(uint16_t x, uint16_t y, const uint16_t *row, uint16_t width)
{
    uint16_t col;

    LCD_Set_Window(x, y, (uint16_t)(x + width - 1U), y);
    for (col = 0; col < width; col++)
    {
        LCD_WriteData_Color(row[col]);
    }
}

/**
 * @brief 统一图片预览入口
 * @param path 图片完整路径
 * @param x 预览区域左上角 X 坐标
 * @param y 预览区域左上角 Y 坐标
 * @param width 预览区域宽度
 * @param height 预览区域高度
 * @return PictureResult 统一图片结果码
 * @details
 * 该函数只做一件事：根据扩展名把请求分发给 BMP 或 JPG 处理函数。
 * 这样 UI 层只关心“显示图片”，而不需要关心格式细节。
 */
PictureResult Picture_Show(const char *path,
                           uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height)
{
    const char *ext = Picture_FindExtension(path);

    if (ext == NULL)
    {
        return PICTURE_ERR_UNSUPPORTED;
    }

    if (Picture_ExtEquals(ext, "bmp"))
    {
        return Picture_ShowBMP(path, x, y, width, height);
    }
    if (Picture_ExtEquals(ext, "jpg") || Picture_ExtEquals(ext, "jpeg"))
    {
        return Picture_ShowJPG(path, x, y, width, height);
    }

    return PICTURE_ERR_UNSUPPORTED;
}

/**
 * @brief 从 SD 卡读取 BMP 并显示到预览区域
 * @details
 * 当前 BMP 路径的主要步骤是：
 * 1. 读取文件头和信息头；
 * 2. 解析宽高、位深、压缩方式；
 * 3. 计算图片在预览区域中的缩放后尺寸与居中位置；
 * 4. 逐行读取源图像素；
 * 5. 必要时做最近邻缩放；
 * 6. 转换为 RGB565 后逐行写入 LCD。
 *
 * 这里采用逐行处理而不是整图加载，是为了控制 MCU RAM 占用。
 */
PictureResult Picture_ShowBMP(const char *path,
                              uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height)
{
    FIL file;
    FRESULT fres;
    UINT bytes_read = 0U;
    uint8_t file_header[BMP_FILE_HEADER_SIZE];
    uint8_t info_header[BMP_INFO_HEADER_SIZE];
    static uint8_t row_bytes[BMP_MAX_ROW_BUFFER_BYTES];
    uint16_t row_rgb565[PICTURE_MAX_DEST_WIDTH];
    uint32_t pixel_data_offset;
    uint32_t dib_header_size;
    int32_t image_width_signed;
    int32_t image_height_signed;
    uint32_t image_width;
    uint32_t image_height;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t row_stride;
    uint32_t dst_row_index;
    uint8_t bottom_up = 1U;
    PictureLayout layout;

    if (path == NULL)
    {
        return PICTURE_ERR_OPEN;
    }
    if (x >= tftlcd_data.width || y >= tftlcd_data.height || width == 0U || height == 0U)
    {
        return PICTURE_ERR_RANGE;
    }

    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        return PICTURE_ERR_OPEN;
    }

    fres = f_read(&file, file_header, sizeof(file_header), &bytes_read);
    if (fres != FR_OK || bytes_read != sizeof(file_header))
    {
        (void)f_close(&file);
        return PICTURE_ERR_READ;
    }
    if (file_header[0] != 'B' || file_header[1] != 'M')
    {
        (void)f_close(&file);
        return PICTURE_ERR_FORMAT;
    }
    pixel_data_offset = Picture_ReadLe32(&file_header[10]);

    fres = f_read(&file, info_header, sizeof(info_header), &bytes_read);
    if (fres != FR_OK || bytes_read != sizeof(info_header))
    {
        (void)f_close(&file);
        return PICTURE_ERR_READ;
    }

    dib_header_size = Picture_ReadLe32(&info_header[0]);
    if (dib_header_size < BMP_INFO_HEADER_SIZE)
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }

    image_width_signed = Picture_ReadLe32Signed(&info_header[4]);
    image_height_signed = Picture_ReadLe32Signed(&info_header[8]);
    bit_count = Picture_ReadLe16(&info_header[14]);
    compression = Picture_ReadLe32(&info_header[16]);

    if (image_width_signed <= 0 || image_height_signed == 0)
    {
        (void)f_close(&file);
        return PICTURE_ERR_FORMAT;
    }

    image_width = (uint32_t)image_width_signed;
    if (image_height_signed < 0)
    {
        bottom_up = 0U;
        image_height = (uint32_t)(-image_height_signed);
    }
    else
    {
        image_height = (uint32_t)image_height_signed;
    }

    if (bit_count == 24U)
    {
        if (compression != BMP_COMPRESSION_RGB)
        {
            (void)f_close(&file);
            return PICTURE_ERR_UNSUPPORTED;
        }
        row_stride = ((image_width * 3U) + 3U) & ~3U;
    }
    else if (bit_count == 16U)
    {
        if (compression != BMP_COMPRESSION_RGB && compression != BMP_COMPRESSION_BITFIELDS)
        {
            (void)f_close(&file);
            return PICTURE_ERR_UNSUPPORTED;
        }
        row_stride = ((image_width * 2U) + 3U) & ~3U;
    }
    else
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }

    if (row_stride > BMP_MAX_ROW_BUFFER_BYTES)
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }
    if (!Picture_ComputeLayout(image_width, image_height, x, y, width, height, &layout))
    {
        (void)f_close(&file);
        return PICTURE_ERR_RANGE;
    }
    if (layout.draw_width > PICTURE_MAX_DEST_WIDTH)
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }

    for (dst_row_index = 0U; dst_row_index < layout.draw_height; dst_row_index++)
    {
        uint32_t src_row_index = (dst_row_index * image_height) / layout.draw_height;
        uint32_t bmp_row_index = bottom_up ? (image_height - 1U - src_row_index) : src_row_index;
        uint32_t row_offset = pixel_data_offset + bmp_row_index * row_stride;
        uint16_t dst_col;

        fres = f_lseek(&file, row_offset);
        if (fres != FR_OK)
        {
            (void)f_close(&file);
            return PICTURE_ERR_READ;
        }

        fres = f_read(&file, row_bytes, (UINT)row_stride, &bytes_read);
        if (fres != FR_OK || bytes_read != (UINT)row_stride)
        {
            (void)f_close(&file);
            return PICTURE_ERR_READ;
        }

        for (dst_col = 0U; dst_col < layout.draw_width; dst_col++)
        {
            uint32_t src_col = ((uint32_t)dst_col * image_width) / layout.draw_width;

            if (bit_count == 24U)
            {
                row_rgb565[dst_col] = Picture_Bgr888ToRgb565(&row_bytes[src_col * 3U]);
            }
            else
            {
                uint16_t pixel = Picture_ReadLe16(&row_bytes[src_col * 2U]);
                row_rgb565[dst_col] = Picture_Bgr555ToRgb565(pixel);
            }
        }

        Picture_DrawRow(layout.draw_x,
                        (uint16_t)(layout.draw_y + dst_row_index),
                        row_rgb565,
                        layout.draw_width);
    }

    (void)f_close(&file);
    return PICTURE_OK;
}

/**
 * @brief 从 SD 卡读取 JPG/JPEG 并显示到预览区域
 * @details
 * JPG 路径本身不在这里直接解码，而是转交给 jpeg_decoder 模块。
 * 本函数主要负责参数统一和错误码映射，使上层可以把 BMP/JPG 看成同一类图片能力。
 */
PictureResult Picture_ShowJPG(const char *path,
                              uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height)
{
    JpegDecoderResult result = JpegDecoder_ShowFile(path, x, y, width, height);

    switch (result)
    {
        case JPEG_DECODER_OK:
            return PICTURE_OK;
        case JPEG_DECODER_ERR_OPEN:
            return PICTURE_ERR_OPEN;
        case JPEG_DECODER_ERR_FORMAT:
            return PICTURE_ERR_JPG_FORMAT;
        case JPEG_DECODER_ERR_READ:
            return PICTURE_ERR_READ;
        case JPEG_DECODER_ERR_RANGE:
            return PICTURE_ERR_RANGE;
        case JPEG_DECODER_ERR_NOMEM:
            return PICTURE_ERR_JPG_NOMEM;
        case JPEG_DECODER_ERR_UNSUPPORTED:
            return PICTURE_ERR_JPG_UNSUPPORTED;
        default:
            return PICTURE_ERR_UNSUPPORTED;
    }
}

const char *Picture_ResultString(PictureResult result)
{
    switch (result)
    {
        case PICTURE_OK:
            return "IMG OK";
        case PICTURE_ERR_OPEN:
            return "OPEN FAIL";
        case PICTURE_ERR_FORMAT:
            return "FORMAT ERR";
        case PICTURE_ERR_READ:
            return "READ ERR";
        case PICTURE_ERR_UNSUPPORTED:
            return "UNSUPPORTED";
        case PICTURE_ERR_RANGE:
            return "RANGE ERR";
        case PICTURE_ERR_JPG_FORMAT:
            return "JPG FORMAT";
        case PICTURE_ERR_JPG_UNSUPPORTED:
            return "JPG UNSUP";
        case PICTURE_ERR_JPG_NOMEM:
            return "JPG NOMEM";
        default:
            return "UNKNOWN";
    }
}
