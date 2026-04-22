# font_storage 模块说明

## 作用

`font_storage` 负责字库在外部 Flash（EN25Q128）中的管理：
- 启动时校验字库头，判断字库是否可用
- 从 SD 文件导入字库到外部 Flash
- 按偏移读取 16x16 字模（32 字节）

## 主要文件

- `font_storage.h`
- `font_storage.c`

## 存储布局

从 `FONT_STORAGE_FLASH_BASE` 开始：
1. `FontStorageHeader`（字库头）
2. 字库正文数据

字库头字段：
- `magic`：魔数
- `version`：版本
- `font_size`：字库字节数
- `crc32`：导入时计算的 CRC32

## 主要接口

- `FontStorage_Init()`：读取并校验字库头
- `FontStorage_IsReady()`：查询字库可用状态
- `FontStorage_GetFontSize()`：获取字库大小
- `FontStorage_ImportFromSD(const char *path)`：从 SD 导入字库
- `FontStorage_ReadGlyph16(uint32_t glyph_offset, uint8_t *glyph_buf32)`：按偏移读取字模

## 典型调用顺序

1. 系统启动后调用 `FontStorage_Init()`
2. 若 `FontStorage_IsReady()==0`，则尝试 `FontStorage_ImportFromSD("0:/Font/HZK16")`
3. 显示阶段通过 `FontStorage_ReadGlyph16()` 读取字模

## 常见错误码

- `FONT_STORAGE_ERR_SD_OPEN`：SD 文件路径错误或文件不存在
- `FONT_STORAGE_ERR_SD_READ`：SD 读取失败
- `FONT_STORAGE_ERR_FORMAT`：文件大小不合法
- `FONT_STORAGE_ERR_NOT_READY`：字库未就绪
- `FONT_STORAGE_ERR_OUT_OF_RANGE`：请求偏移超出字库范围
