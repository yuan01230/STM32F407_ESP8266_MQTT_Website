#ifndef UI_LIST_H
#define UI_LIST_H

/**
 * @brief 显示/刷新文件列表页
 * @details
 * 内部实现“首帧整页 + 后续局部刷新”防闪烁策略。
 */
void UI_ListPage_Show(void);

/**
 * @brief 列表选择下移一项（循环）
 */
void UI_List_SelectDown(void);

/**
 * @brief 列表选择上移一项（循环）
 */
void UI_List_SelectUp(void);

#endif
