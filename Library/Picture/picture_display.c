#include "picture_display.h"

#include "ff.h"
#include "jpeg_decoder.h"
#include "../tftlcd/tftlcd.h"

/**
 * @file picture_display.c
 * @brief 从 SD 卡读取图片并显示到 LCD 的基础驱动实现
 * @details
 * 当前文件实现的是图片显示第一版能力，目标是先稳定支持 BMP 图片。
 * 整体处理流程如下：
 * 1. 通过 FATFS 打开 SD 卡中的 BMP 文件；
 * 2. 读取并解析 BMP 文件头和信息头；
 * 3. 判断图片宽高、位深、压缩方式是否合法；
 * 4. 根据 BMP 的行存储方式逐行定位像素数据；
 * 5. 将读取到的像素转换为 LCD 使用的 RGB565；
 * 6. 逐行写入 LCD，避免整图缓存带来的 RAM 压力。
 *
 * 这一版重点保证流程可用、错误明确、便于后续继续扩展到 picture 文件夹浏览、
 * RGB565 原始图显示或更多图片格式支持。
 */

/** BMP 文件头固定长度。 */
#define BMP_FILE_HEADER_SIZE 14U
/** 常见 BITMAPINFOHEADER 长度。 */
#define BMP_INFO_HEADER_SIZE 40U
/** BMP 无压缩标志。 */
#define BMP_COMPRESSION_RGB 0U
/** BMP 位域压缩标志，当前仅做简单兼容。 */
#define BMP_COMPRESSION_BITFIELDS 3U

/**
 * @brief 当前实现允许显示的最大可见宽度
 * @details
 * 当前 LCD 竖屏宽度为 320，因此使用固定大小行缓冲即可覆盖一整行显示。
 * 这样可以避免动态内存分配，简化 MCU 端实现并降低内存碎片风险。
 */
#define BMP_MAX_VISIBLE_WIDTH 320U

/**
 * @brief 读取小端格式的 16 位整数
 * @param buf 指向原始字节缓冲区的指针
 * @return uint16_t 解析后的 16 位整数
 * @details
 * BMP 文件头中的多字节字段采用小端存储，因此需要手动按字节重组。
 */
static uint16_t Picture_ReadLe16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * @brief 读取小端格式的 32 位整数
 * @param buf 指向原始字节缓冲区的指针
 * @return uint32_t 解析后的 32 位整数
 * @details
 * 该函数主要用于解析 BMP 中的偏移量、宽度、高度、压缩方式等字段。
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
 * @param buf 指向原始字节缓冲区的指针
 * @return int32_t 解析后的有符号 32 位整数
 * @details
 * BMP 的高度字段允许为负值：
 * - 大于 0：表示图像按 bottom-up 方式存储；
 * - 小于 0：表示图像按 top-down 方式存储。
 * 因此这里必须保留符号信息。
 */
static int32_t Picture_ReadLe32Signed(const uint8_t *buf)
{
    return (int32_t)Picture_ReadLe32(buf);
}

/**
 * @brief 将 BMP 中 24 位 BGR 像素转换为 RGB565
 * @param pixel 指向单个像素 3 字节数据的指针，顺序为 B、G、R
 * @return uint16_t 转换后的 RGB565 像素值
 * @details
 * BMP 常见 24 位像素存储顺序为 BGR，而 LCD 更适合直接接收 RGB565。
 * 因此这里将 8 位 R/G/B 分量压缩到 5/6/5 位格式。
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
 * @param pixel BMP 文件中读取出的 16 位像素值
 * @return uint16_t 转换后的 RGB565 像素值
 * @details
 * 当前实现按常见的 5-5-5 布局进行近似兼容处理，
 * 然后将绿色分量扩展到 RGB565 需要的 6 位。
 * 这不是完整位域解码器，但足以覆盖部分简单 16 位 BMP 场景。
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
 * @brief 判断两个扩展名是否在忽略大小写后相等
 * @param lhs 左侧扩展名，不包含点号
 * @param rhs 右侧扩展名，不包含点号
 * @return uint8_t 1=相等，0=不相等
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
 * @brief 从完整路径中提取文件扩展名
 * @param path 完整文件路径
 * @return const char* 指向扩展名的指针；没有扩展名时返回 NULL
 * @details
 * 这里返回的是点号后面的内容，例如：
 * 1. "0:/Picture/a.bmp" -> "bmp"
 * 2. "0:/Picture/a.jpeg" -> "jpeg"
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
 * @brief 将一行 RGB565 像素数据写入 LCD
 * @param x 当前行起始显示 X 坐标
 * @param y 当前行显示 Y 坐标
 * @param row 当前行 RGB565 像素缓冲区
 * @param width 当前行需要显示的像素个数
 * @details
 * 图片显示采用逐行刷新策略：
 * 1. 先设置 LCD 窗口为一整行；
 * 2. 再连续写入该行的所有 RGB565 像素。
 * 这种方式比逐点绘制效率更高，也更适合大图显示。
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
 * @brief 统一图片显示入口
 * @param path 图片完整路径
 * @param x 图片左上角显示起点 X 坐标
 * @param y 图片左上角显示起点 Y 坐标
 * @return PictureResult 图片显示执行结果
 * @details
 * 当前根据扩展名自动选择底层解码路径：
 * 1. bmp -> BMP 逐行解析显示
 * 2. jpg/jpeg -> TJpgDec 运行时解码显示
 */
PictureResult Picture_Show(const char *path, uint16_t x, uint16_t y)
{
    const char *ext = Picture_FindExtension(path);

    if (ext == NULL)
    {
        return PICTURE_ERR_UNSUPPORTED;
    }

    if (Picture_ExtEquals(ext, "bmp"))
    {
        return Picture_ShowBMP(path, x, y);
    }
    if (Picture_ExtEquals(ext, "jpg") || Picture_ExtEquals(ext, "jpeg"))
    {
        return Picture_ShowJPG(path, x, y);
    }

    return PICTURE_ERR_UNSUPPORTED;
}

/**
 * @brief 从 SD 卡读取 BMP 文件并显示到 LCD
 * @param path BMP 文件路径
 * @param x 显示起始 X 坐标
 * @param y 显示起始 Y 坐标
 * @return PictureResult 图片显示执行结果
 * @details
 * 该函数是当前图片显示第一版的核心入口，按以下步骤工作：
 * 1. 检查输入参数和显示坐标是否合法；
 * 2. 使用 FATFS 打开 BMP 文件；
 * 3. 读取 BMP 文件头并检查是否为标准 BM 文件；
 * 4. 读取信息头，解析宽高、位深、压缩方式；
 * 5. 判断图片是否属于当前驱动支持范围；
 * 6. 根据 LCD 剩余可显示区域计算实际可见宽高；
 * 7. 逐行定位 BMP 数据位置；
 * 8. 逐行读取像素并转换为 RGB565；
 * 9. 立即写入 LCD 对应行；
 * 10. 全部完成后关闭文件并返回状态码。
 */
PictureResult Picture_ShowBMP(const char *path, uint16_t x, uint16_t y)
{
    FIL file;
    FRESULT fres;
    UINT bytes_read = 0;
    uint8_t file_header[BMP_FILE_HEADER_SIZE];
    uint8_t info_header[BMP_INFO_HEADER_SIZE];
    uint8_t row_bytes[BMP_MAX_VISIBLE_WIDTH * 3U];
    uint16_t row_rgb565[BMP_MAX_VISIBLE_WIDTH];
    uint32_t pixel_data_offset;
    uint32_t dib_header_size;
    int32_t image_width_signed;
    int32_t image_height_signed;
    uint32_t image_width;
    uint32_t image_height;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t row_stride;
    uint32_t row_index;
    uint16_t visible_width;
    uint16_t visible_height;
    uint8_t bottom_up = 1U;

    /* 第 1 步：检查基础参数。
     * 如果路径为空或显示起点已经超出屏幕范围，就不再继续后续文件访问。 */
    if (path == NULL)
    {
        return PICTURE_ERR_OPEN;
    }
    if (x >= tftlcd_data.width || y >= tftlcd_data.height)
    {
        return PICTURE_ERR_RANGE;
    }

    /* 第 2 步：打开 BMP 文件。
     * 这里依赖 FATFS，如果打开失败通常说明路径错误、文件不存在或 SD 状态异常。 */
    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        return PICTURE_ERR_OPEN;
    }

    /* 第 3 步：读取 BMP 文件头。
     * 文件头中包含 BMP 标识和像素数据起始偏移。 */
    fres = f_read(&file, file_header, sizeof(file_header), &bytes_read);
    if (fres != FR_OK || bytes_read != sizeof(file_header))
    {
        (void)f_close(&file);
        return PICTURE_ERR_READ;
    }

    /* 第 4 步：检查 BMP 文件标识。
     * 标准 BMP 文件头前两个字节必须是 'B' 'M'。 */
    if (file_header[0] != 'B' || file_header[1] != 'M')
    {
        (void)f_close(&file);
        return PICTURE_ERR_FORMAT;
    }

    /* 第 5 步：记录像素数据在文件中的起始偏移。 */
    pixel_data_offset = Picture_ReadLe32(&file_header[10]);

    /* 第 6 步：读取 BMP 信息头。
     * 该头部描述宽度、高度、位深、压缩方式等核心显示信息。 */
    fres = f_read(&file, info_header, sizeof(info_header), &bytes_read);
    if (fres != FR_OK || bytes_read != sizeof(info_header))
    {
        (void)f_close(&file);
        return PICTURE_ERR_READ;
    }

    /* 第 7 步：检查 DIB 头长度是否属于当前实现可处理范围。 */
    dib_header_size = Picture_ReadLe32(&info_header[0]);
    if (dib_header_size < BMP_INFO_HEADER_SIZE)
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }

    /* 第 8 步：解析宽高、位深和压缩方式。 */
    image_width_signed = Picture_ReadLe32Signed(&info_header[4]);
    image_height_signed = Picture_ReadLe32Signed(&info_header[8]);
    bit_count = Picture_ReadLe16(&info_header[14]);
    compression = Picture_ReadLe32(&info_header[16]);

    /* 第 9 步：检查宽高是否合法。
     * 宽度必须大于 0，高度不能为 0。 */
    if (image_width_signed <= 0 || image_height_signed == 0)
    {
        (void)f_close(&file);
        return PICTURE_ERR_FORMAT;
    }

    image_width = (uint32_t)image_width_signed;
    if (image_height_signed < 0)
    {
        /* 第 10 步：处理 top-down BMP。
         * 高度为负时，文件中的第一行就是图像顶部。 */
        bottom_up = 0U;
        image_height = (uint32_t)(-image_height_signed);
    }
    else
    {
        /* 第 11 步：处理标准 bottom-up BMP。
         * 高度为正时，文件中的第一行对应图像底部。 */
        image_height = (uint32_t)image_height_signed;
    }

    /* 第 12 步：判断图片格式是否属于当前第一版支持范围。
     * 当前重点支持无压缩 24 位 BMP，16 位 BMP 做简单兼容。
     * 同时计算每一行在文件中的实际字节数，并考虑 4 字节对齐。 */
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
        row_stride = (image_width * 2U + 3U) & ~3U;
    }
    else
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }

    /* 第 13 步：根据屏幕剩余空间计算真正需要显示的宽高。
     * 如果图片大于屏幕，则只显示可见部分，不做缩放。 */
    visible_width = (uint16_t)(((uint32_t)(tftlcd_data.width - x) < image_width) ? (tftlcd_data.width - x) : image_width);
    visible_height = (uint16_t)(((uint32_t)(tftlcd_data.height - y) < image_height) ? (tftlcd_data.height - y) : image_height);
    if (visible_width == 0U || visible_height == 0U)
    {
        (void)f_close(&file);
        return PICTURE_ERR_RANGE;
    }

    /* 第 14 步：检查固定行缓冲是否足够容纳一行可见数据。 */
    if (visible_width > BMP_MAX_VISIBLE_WIDTH)
    {
        (void)f_close(&file);
        return PICTURE_ERR_UNSUPPORTED;
    }

    /* 第 15 步：开始逐行显示。
     * 每次循环只处理一行，这样内存占用稳定且易于调试。 */
    for (row_index = 0; row_index < visible_height; row_index++)
    {
        uint32_t bmp_row_index = bottom_up ? (image_height - 1U - row_index) : row_index;
        uint32_t row_offset = pixel_data_offset + bmp_row_index * row_stride;

        /* 第 16 步：定位到当前 BMP 行的数据偏移。 */
        fres = f_lseek(&file, row_offset);
        if (fres != FR_OK)
        {
            (void)f_close(&file);
            return PICTURE_ERR_READ;
        }

        if (bit_count == 24U)
        {
            uint16_t col;
            UINT need_read = (UINT)(visible_width * 3U);

            /* 第 17 步：读取当前 24 位 BMP 的一整行像素。 */
            fres = f_read(&file, row_bytes, need_read, &bytes_read);
            if (fres != FR_OK || bytes_read != need_read)
            {
                (void)f_close(&file);
                return PICTURE_ERR_READ;
            }

            /* 第 18 步：将当前行所有 BGR888 像素转换为 RGB565。 */
            for (col = 0; col < visible_width; col++)
            {
                row_rgb565[col] = Picture_Bgr888ToRgb565(&row_bytes[col * 3U]);
            }
        }
        else
        {
            uint16_t col;
            UINT need_read = (UINT)(visible_width * 2U);

            /* 第 19 步：读取当前 16 位 BMP 的一整行像素。 */
            fres = f_read(&file, row_bytes, need_read, &bytes_read);
            if (fres != FR_OK || bytes_read != need_read)
            {
                (void)f_close(&file);
                return PICTURE_ERR_READ;
            }

            /* 第 20 步：将当前行 16 位像素转换为 RGB565。 */
            for (col = 0; col < visible_width; col++)
            {
                uint16_t pixel = Picture_ReadLe16(&row_bytes[col * 2U]);
                row_rgb565[col] = Picture_Bgr555ToRgb565(pixel);
            }
        }

        /* 第 21 步：把当前行像素直接写入 LCD 对应位置。 */
        Picture_DrawRow(x, (uint16_t)(y + row_index), row_rgb565, visible_width);
    }

    /* 第 22 步：全部显示完成后关闭文件并返回成功。 */
    (void)f_close(&file);
    return PICTURE_OK;
}

/**
 * @brief 从 SD 卡读取 JPEG 文件并显示到 LCD
 * @param path JPG/JPEG 文件路径
 * @param x 图片左上角显示起始 X 坐标
 * @param y 图片左上角显示起始 Y 坐标
 * @return PictureResult 图片显示结果状态码
 * @details
 * JPEG 的底层解码已经在独立模块中完成，这里只负责：
 * 1. 统一对上层暴露 PictureResult；
 * 2. 把 JPEG 解码层的细分错误状态映射成工程已有错误码。
 */
PictureResult Picture_ShowJPG(const char *path, uint16_t x, uint16_t y)
{
    JpegDecoderResult result = JpegDecoder_ShowFile(path, x, y);

    switch (result)
    {
        case JPEG_DECODER_OK:
            return PICTURE_OK;
        case JPEG_DECODER_ERR_OPEN:
            return PICTURE_ERR_OPEN;
        case JPEG_DECODER_ERR_FORMAT:
            return PICTURE_ERR_FORMAT;
        case JPEG_DECODER_ERR_READ:
            return PICTURE_ERR_READ;
        case JPEG_DECODER_ERR_RANGE:
            return PICTURE_ERR_RANGE;
        case JPEG_DECODER_ERR_UNSUPPORTED:
        case JPEG_DECODER_ERR_NOMEM:
        default:
            return PICTURE_ERR_UNSUPPORTED;
    }
}

/**
 * @brief 将执行结果转换为可显示的简短文本
 * @param result 图片显示执行结果
 * @return const char* 状态字符串
 * @details
 * 该字符串主要用于 UI 底部状态栏显示，便于快速定位当前图片加载问题。
 */
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
        default:
            return "UNKNOWN";
    }
}
