#ifndef FONT_CODEC_H
#define FONT_CODEC_H

#include <stdint.h>

/*
 * 编码解析结果：
 * - FONT_CODEC_OK: 解析到一个合法的 GB2312 双字节汉字
 * - FONT_CODEC_ASCII: 解析到一个 ASCII 字符
 * - 其他: 参数或字节序列非法
 */
typedef enum
{
    FONT_CODEC_OK = 0,
    FONT_CODEC_ASCII,
    FONT_CODEC_ERR_PARAM,
    FONT_CODEC_ERR_INVALID_SEQ,
    FONT_CODEC_ERR_UNSUPPORTED
} FontCodecResult;

/*
 * 单个字符解析结果：
 * - is_ascii: 1 表示 ASCII，0 表示 GB2312 汉字
 * - ascii_char: ASCII 字符值（仅 is_ascii=1 时有效）
 * - consumed: 本次从输入流中消耗的字节数（ASCII=1，汉字=2）
 * - glyph_offset: 对应 HZK16 字库中的字模偏移（仅汉字有效）
 */
typedef struct
{
    uint8_t is_ascii;
    uint8_t ascii_char;
    uint16_t consumed;
    uint32_t glyph_offset;
} FontCodecToken;

/*
 * 解析 text 当前指向的字符（ASCII 或 GB2312 双字节）。
 *
 * 参数：
 * - text: 输入字节流指针
 * - token: 输出解析结果
 *
 * 返回：
 * - FONT_CODEC_ASCII / FONT_CODEC_OK 表示解析成功
 * - 其他错误码表示参数或编码序列不合法
 */
FontCodecResult FontCodec_ParseGB2312(const uint8_t *text, FontCodecToken *token);

#endif
