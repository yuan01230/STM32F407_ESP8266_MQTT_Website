# 外部 Flash 字库启动设计

## 目标

解决当前工程中文无法稳定显示的问题：

- 上电时自动检查外部 Flash（EN25Q128）中的 `HZK16` 字库是否可用
- 若字库不可用且 SD 已挂载，则从 `0:/Font/HZK16` 自动导入到外部 Flash
- 运行时中文显示统一走 `font_codec + font_storage` 链路

## 现状问题

- `LCD_ShowTextMixed()` 依赖 `FontStorage_ReadGlyph16()`，但 `main.c` 未初始化字库链路
- `LCD_ShowChinese()` 仍使用 `font.h` 中的内置小字库，与外部 Flash 字库方案割裂
- `ui_font_import.c` 已有上电导入逻辑，但耦合在 UI 模块，不适合作为基础启动依赖

## 方案

新增独立字库启动模块，负责：

1. 初始化 EN25Q128
2. 挂载 SD 后检查字库头
3. 外部 Flash 字库有效时直接返回
4. 外部 Flash 字库无效且 SD 可用时，从 `0:/Font/HZK16` 自动导入
5. 向 `main.c` 返回 SD/字库就绪状态和错误码

同时统一中文显示接口：

- `LCD_ShowTextMixed()` 保持现有实现
- `LCD_ShowChinese()` 改为内部复用 `font_codec + font_storage`
- 不再依赖 `font.h` 的内置中文字模做主链路显示

## 结果预期

- 首次上电：若 SD 卡存在字库文件，则自动导入到 EN25Q128
- 后续上电：若外部 Flash 字库头有效，则直接使用，不重复导入
- `main.c` 中 `KEY2` 的中文测试可直接显示，不再出现占位框或空白
