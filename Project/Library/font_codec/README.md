# font_codec 模块说明

## 作用

`font_codec` 用于把输入字节流解析为：
- ASCII 单字节字符
- GB2312 双字节汉字

并为汉字计算其在 `HZK16` 字库中的字模偏移。

## 主要文件

- `font_codec.h`
- `font_codec.c`

## 主要接口

- `FontCodec_ParseGB2312(const uint8_t *text, FontCodecToken *token)`

返回值：
- `FONT_CODEC_ASCII`：解析到 ASCII
- `FONT_CODEC_OK`：解析到 GB2312 汉字
- 其他错误码：参数非法或字节序列非法

## 偏移规则

对 GB2312 字符（区码 `qh`、位码 `wh`）：

`((qh - 0xA1) * 94 + (wh - 0xA1)) * 32`

说明：`HZK16` 中每个 16x16 字模固定占用 32 字节。

## 使用建议

- 输入中文文本请确保为 GB2312 编码。
- 若上层显示出现占位框，先检查：
  1. 字节流编码是否正确
  2. 字库文件是否与当前偏移规则匹配
