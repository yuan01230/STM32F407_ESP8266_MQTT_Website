#ifndef KEY_H
#define KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief 按键编号定义
 * @note
 * - 枚举顺序必须与板级配置表 `BOARD_KEY_CONFIG_TABLE` 顺序一致。
 * - 若扩展按键数量，请同步更新枚举与板级配置。
 */
typedef enum {
    KEY_NONE = -1, /**< 无按键事件 */
    KEY0 = 0,      /**< KEY0 事件 */
    KEY1 = 1,      /**< KEY1 事件 */
    KEY2 = 2,      /**< KEY2 事件 */
    KEY_UP = 3     /**< KEY_UP 事件 */
} KeyName_t;

/**
 * @brief 按键默认消抖时间（单位：毫秒）
 * @note 可根据按键硬件质量和扫描周期进行调整
 */
#define KEY_DEFAULT_DEBOUNCE_MS 15U

/**
 * @brief 初始化按键驱动运行时状态
 * @note
 * - 建议在系统初始化完成后调用一次
 * - 会读取当前按键电平作为初始稳定状态，避免上电误触发
 */
void Key_Init(void);

/**
 * @brief 扫描一次按键状态，返回单次按下事件
 * @return KeyName_t 触发的按键编号；无事件返回 KEY_NONE
 * @note
 * - 该接口为非阻塞轮询接口
 * - 建议在主循环中每 5~10ms 调用一次
 * - 同一按键按住不放只会上报一次，释放后才允许下一次触发
 */
KeyName_t Key_Scan(void);

#ifdef __cplusplus
}
#endif

#endif
