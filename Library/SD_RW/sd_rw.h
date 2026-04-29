#ifndef SD_RW_H
#define SD_RW_H

#include <stdint.h>

/**
 * @file sd_rw.h
 * @brief SD 文件操作相关的 LCD 菜单展示接口
 * @details
 * 该模块负责把一些典型的 SD 文件操作结果或入口信息显示到 LCD。
 * 当前接口偏展示层，常用于主流程中快速调用固定页面模板。
 */

/**
 * @brief 显示存储空间信息页面
 */
void LCD_Show_Storage_Size_Menu(void);

/**
 * @brief 显示“创建文档”结果页面
 */
void LCD_Show_Create_Doc_Menu(void);

/**
 * @brief 显示“读取文档”结果页面
 */
void LCD_Show_Read_Doc_Menu(void);

/**
 * @brief 显示文件列表相关页面
 */
void LCD_Show_List_Files_Menu(void);

#endif

