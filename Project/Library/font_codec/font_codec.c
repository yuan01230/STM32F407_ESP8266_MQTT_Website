#include "font_codec.h"

/*
 * 说明：
 * - 本文件只解决一个问题：把当前输入字节流解释成“ASCII 字符”或“GB2312 汉字”。
 * - 一旦识别成功，就给出：
 *   1. 本次消费了几个字节；
 *   2. 是否是 ASCII；
 *   3. 如果是汉字，对应 HZK16 字库中的字模偏移是多少。
 */

/**
 * @brief 解析当前输入位置的一个字符
 * @param text 输入字节流
 * @param token 输出解析结果
 * @return FontCodecResult 解析状态
 * @details
 * 处理顺序很直接：
 * 1. 先做参数合法性检查；
 * 2. 如果首字节最高位为 0，则按 ASCII 单字节处理；
 * 3. 否则按 GB2312 双字节区位码规则校验；
 * 4. 对合法汉字计算其在 HZK16 字库中的偏移地址。
 */
FontCodecResult FontCodec_ParseGB2312(const uint8_t *text, FontCodecToken *token)
{
    uint8_t qh;
    uint8_t wh;

    /* 参数有效性检查 */
    if (text == 0 || token == 0)
    {
        return FONT_CODEC_ERR_PARAM;
    }

    /* ASCII：高位为 0，直接按单字节处理 */
    if (text[0] < 0x80)
    {
        token->is_ascii = 1;
        token->ascii_char = text[0];
        token->consumed = 1;
        token->glyph_offset = 0;
        return FONT_CODEC_ASCII;
    }

    /* 非 ASCII：按 GB2312 双字节解析 */
    qh = text[0];
    wh = text[1];

    /*
     * GB2312 有效区间：
     * - 区码 qh: 0xA1 ~ 0xF7
     * - 位码 wh: 0xA1 ~ 0xFE
     */
    if (qh < 0xA1 || qh > 0xF7 || wh < 0xA1 || wh > 0xFE)
    {
        return FONT_CODEC_ERR_INVALID_SEQ;
    }

    token->is_ascii = 0;
    token->ascii_char = 0;
    token->consumed = 2;

    /*
     * HZK16 偏移计算公式：
     * ((区码-0xA1)*94 + (位码-0xA1)) * 32
     * 每个 16x16 字模占 32 字节。
     */
    token->glyph_offset = ((uint32_t)(qh - 0xA1) * 94U + (uint32_t)(wh - 0xA1)) * 32U;
    return FONT_CODEC_OK;
}
