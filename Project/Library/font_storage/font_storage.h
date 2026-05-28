#ifndef FONT_STORAGE_H
#define FONT_STORAGE_H

#include <stdint.h>

/*
 * 字库存储模块返回码：
 * - FONT_STORAGE_OK: 操作成功
 * - FONT_STORAGE_ERR_SD_OPEN/SD_READ: SD 文件打开或读取失败
 * - FONT_STORAGE_ERR_NOT_READY: 字库尚未就绪
 * - FONT_STORAGE_ERR_OUT_OF_RANGE: 字模偏移越界
 * - FONT_STORAGE_ERR_FORMAT: 字库大小/格式不符合约束
 */
typedef enum
{
    FONT_STORAGE_OK = 0,
    FONT_STORAGE_ERR_PARAM,
    FONT_STORAGE_ERR_SD_OPEN,
    FONT_STORAGE_ERR_SD_READ,
    FONT_STORAGE_ERR_SD_SEEK,
    FONT_STORAGE_ERR_FLASH_IO,
    FONT_STORAGE_ERR_NOT_READY,
    FONT_STORAGE_ERR_OUT_OF_RANGE,
    FONT_STORAGE_ERR_FORMAT
} FontStorageResult;

/* 字库导入进度回调：done/total 为已写入与总字节数 */
typedef void (*FontStorageProgressCallback)(uint32_t done, uint32_t total, void *user);

/* 启动时初始化：从外部 Flash 读取字库头并判断字库是否可用 */
void FontStorage_Init(void);

/* 查询字库是否可用（1=可用，0=不可用） */
uint8_t FontStorage_IsReady(void);

/* 获取当前字库大小（字节），未就绪时返回 0 */
uint32_t FontStorage_GetFontSize(void);

/* 从 SD 指定路径导入字库文件到外部 Flash */
FontStorageResult FontStorage_ImportFromSD(const char *path);

/* 从 SD 导入字库（带进度回调） */
FontStorageResult FontStorage_ImportFromSDEx(const char *path, FontStorageProgressCallback cb, void *user);

/* 从外部 Flash 按偏移读取 16x16 字模（32 字节） */
FontStorageResult FontStorage_ReadGlyph16(uint32_t glyph_offset, uint8_t *glyph_buf32);

#endif
