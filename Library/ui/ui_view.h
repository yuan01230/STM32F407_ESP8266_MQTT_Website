#ifndef UI_VIEW_H
#define UI_VIEW_H

#include <stdint.h>

/**
 * @brief 打开当前选中文件并切换到查看页
 */
void UI_View_OpenSelected(void);

/**
 * @brief 显示指定索引文件内容
 * @param index 文件索引
 */
void UI_View_Show(uint16_t index);

#endif
