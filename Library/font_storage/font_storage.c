#include "font_storage.h"

#include "../../Library/EN25Q128/EN25Q128.h"
#include "../../FATFS/App/fatfs.h"

#include <string.h>

/*
 * 说明：
 * - 本文件负责“字库如何落到外部 Flash，并在运行时按偏移读回字模”。
 * - 上层只关心三件事：
 *   1. 字库是否已经就绪；
 *   2. 能否从 SD 重新导入字库；
 *   3. 给定字模偏移后，能否读到对应 16x16 点阵数据。
 */

/* 字库头魔数（'FZ16'） */
#define FONT_STORAGE_MAGIC           0x465A3136UL
/* 字库头版本号，用于兼容性判断 */
#define FONT_STORAGE_VERSION         0x00010000UL
/* 字库在 EN25Q128 中的起始地址 */
#define FONT_STORAGE_FLASH_BASE      0x00F00000UL
/* 当前允许的最大字库大小（512KB） */
#define FONT_STORAGE_MAX_SIZE        (512UL * 1024UL)
/* 16x16 字模固定为 32 字节 */
#define FONT_STORAGE_GLYPH_SIZE      32U
/* SD -> Flash 导入时的分块大小 */
#define FONT_STORAGE_COPY_CHUNK      256U

/* Flash 中的字库头布局 */
typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t font_size;
    uint32_t crc32;
    uint32_t reserved0;
    uint32_t reserved1;
} FontStorageHeader;

/* 运行态缓存：是否就绪 + 字库头信息 */
static uint8_t g_font_ready = 0;
static FontStorageHeader g_header;

/* 增量 CRC32 计算（多项式 0xEDB88320） */
static uint32_t FontStorage_Crc32Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* 校验字库头是否合法（magic/version/size） */
static uint8_t FontStorage_ValidateHeader(const FontStorageHeader *header)
{
    if (header->magic != FONT_STORAGE_MAGIC)
    {
        return 0;
    }

    if (header->version != FONT_STORAGE_VERSION)
    {
        return 0;
    }

    if (header->font_size == 0 || header->font_size > FONT_STORAGE_MAX_SIZE)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 启动时初始化字库状态
 * @details
 * 系统上电后不会立刻重导字库，而是先去外部 Flash 读取字库头。
 * 如果头信息合法，就认为当前板子上已经有可用字库，可以直接进入运行态。
 */
void FontStorage_Init(void)
{
    EN25QXX_Read((uint8_t *)&g_header, FONT_STORAGE_FLASH_BASE, (uint16_t)sizeof(g_header));
    g_font_ready = FontStorage_ValidateHeader(&g_header);
}

uint8_t FontStorage_IsReady(void)
{
    return g_font_ready;
}

uint32_t FontStorage_GetFontSize(void)
{
    if (!g_font_ready)
    {
        return 0;
    }

    return g_header.font_size;
}

/**
 * @brief 从 SD 导入字库到外部 Flash，并支持进度回调
 * @param path SD 卡中字库文件路径
 * @param cb 进度回调
 * @param user 回调上下文
 * @return FontStorageResult 导入结果
 * @details
 * 导入流程如下：
 * 1. 打开 SD 字库文件；
 * 2. 校验文件大小是否合法；
 * 3. 计算需要擦除的 Flash 扇区数量；
 * 4. 分块读取 SD 文件并写入 Flash；
 * 5. 计算整个字库文件的 CRC32；
 * 6. 最后写入字库头，标记此次导入完成。
 */
FontStorageResult FontStorage_ImportFromSDEx(const char *path, FontStorageProgressCallback cb, void *user)
{
    FIL file;
    FRESULT fres;
    UINT read_len;
    uint8_t buf[FONT_STORAGE_COPY_CHUNK];
    uint32_t total_size;
    uint32_t write_addr;
    uint32_t remain;
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    uint32_t sector_start;
    uint32_t sector_count;
    FontStorageHeader header;

    if (path == 0)
    {
        return FONT_STORAGE_ERR_PARAM;
    }

    /* 打开 SD 字库文件 */
    fres = f_open(&file, path, FA_READ);
    if (fres != FR_OK)
    {
        return FONT_STORAGE_ERR_SD_OPEN;
    }

    /* 文件大小校验（0 < size <= FONT_STORAGE_MAX_SIZE） */
    total_size = f_size(&file);
    if (total_size == 0 || total_size > FONT_STORAGE_MAX_SIZE)
    {
        (void)f_close(&file);
        return FONT_STORAGE_ERR_FORMAT;
    }

    /* 计算需要擦除的扇区数量（字库头 + 正文字库） */
    sector_start = FONT_STORAGE_FLASH_BASE / 4096UL;
    sector_count = (uint32_t)((sizeof(FontStorageHeader) + total_size + 4095UL) / 4096UL);

    for (i = 0; i < sector_count; i++)
    {
        EN25QXX_Erase_Sector(sector_start + i);
    }

    write_addr = FONT_STORAGE_FLASH_BASE + sizeof(FontStorageHeader);
    remain = total_size;
    if (cb != 0)
    {
        cb(0U, total_size, user);
    }

    /* 分块读取 SD 并写入 Flash，同时计算 CRC */
    while (remain > 0)
    {
        UINT chunk = (remain > FONT_STORAGE_COPY_CHUNK) ? FONT_STORAGE_COPY_CHUNK : (UINT)remain;
        read_len = 0;

        fres = f_read(&file, buf, chunk, &read_len);
        if (fres != FR_OK)
        {
            (void)f_close(&file);
            return FONT_STORAGE_ERR_SD_READ;
        }

        if (read_len == 0)
        {
            (void)f_close(&file);
            return FONT_STORAGE_ERR_SD_READ;
        }

        EN25QXX_Write(buf, write_addr + offset, (uint16_t)read_len);

        crc = FontStorage_Crc32Update(crc, buf, read_len);

        offset += read_len;
        remain -= read_len;
        if (cb != 0)
        {
            cb(offset, total_size, user);
        }
    }

    (void)f_close(&file);

    /* 写入字库头，标记本次导入结果 */
    header.magic = FONT_STORAGE_MAGIC;
    header.version = FONT_STORAGE_VERSION;
    header.font_size = total_size;
    header.crc32 = crc ^ 0xFFFFFFFFUL;
    header.reserved0 = 0;
    header.reserved1 = 0;

    EN25QXX_Write((uint8_t *)&header, FONT_STORAGE_FLASH_BASE, (uint16_t)sizeof(header));

    g_header = header;
    g_font_ready = 1;
    return FONT_STORAGE_OK;
}

FontStorageResult FontStorage_ImportFromSD(const char *path)
{
    return FontStorage_ImportFromSDEx(path, 0, 0);
}

/**
 * @brief 按字模偏移读取 16x16 点阵
 * @param glyph_offset 字模偏移
 * @param glyph_buf32 输出 32 字节点阵缓冲区
 * @return FontStorageResult 读取结果
 * @details
 * 该接口是运行时最常用的字库读取入口。
 * 上层在完成编码解析后，会把 GB2312 计算出的字模偏移传进来，
 * 然后再把读出的 32 字节点阵送给 LCD 绘制函数。
 */
FontStorageResult FontStorage_ReadGlyph16(uint32_t glyph_offset, uint8_t *glyph_buf32)
{
    uint32_t addr;

    if (glyph_buf32 == 0)
    {
        return FONT_STORAGE_ERR_PARAM;
    }

    /* 字库未就绪时，禁止读取 */
    if (!g_font_ready)
    {
        return FONT_STORAGE_ERR_NOT_READY;
    }

    /* 防越界：读取区间 [offset, offset+32) 必须落在字库范围内 */
    if (glyph_offset + FONT_STORAGE_GLYPH_SIZE > g_header.font_size)
    {
        return FONT_STORAGE_ERR_OUT_OF_RANGE;
    }

    addr = FONT_STORAGE_FLASH_BASE + sizeof(FontStorageHeader) + glyph_offset;
    EN25QXX_Read(glyph_buf32, addr, FONT_STORAGE_GLYPH_SIZE);

    return FONT_STORAGE_OK;
}
