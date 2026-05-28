#ifndef UI_FONT_IMPORT_H
#define UI_FONT_IMPORT_H

#include <stdint.h>

/**
 * @brief 系统启动阶段字库初始化
 * @param sd_ready SD 挂载状态
 * @details
 * 检查外部 Flash 字库，不可用时尝试从 SD 导入。
 */
void UI_Font_ImportSystemInit(uint8_t sd_ready);

/**
 * @brief 主页面触发字库重导入
 */
void UI_Font_RunImportFromMain(void);

#endif
