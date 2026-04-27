#ifndef UI_VIEW_H
#define UI_VIEW_H

#include <stdint.h>

/**
 * @brief 打开当前选中的普通文件并切换到查看页
 * @details
 * 该接口由列表页的 KEY2 打开文件路径触发。
 * 如果当前高亮项不是普通文件，例如目录或 [..]，函数会直接返回。
 */
void UI_View_OpenSelected(void);

/**
 * @brief 显示指定索引文件的预览内容
 * @param index 文件列表索引
 * @details
 * 当前实现会根据文件扩展名自动决定查看方式：
 * 1. bmp / jpg / jpeg -> 图片预览
 * 2. 其他文件 -> 文本或十六进制摘要预览
 */
void UI_View_Show(uint16_t index);

#endif
