//
// Created by 标语 on 26-1-2.
//

#include "key.h"

// 按键参数配置
#define DEBOUNCE_TIME_MS    15   // 消抖时间（毫秒）
#define SCAN_INTERVAL_MS    10   // 建议主循环每 10ms 调用一次 Key_Scan

// 按键结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
    uint8_t       active_level; // 1=高有效，0=低有效
    uint8_t       state;        // 当前稳定状态：0=释放，1=按下
    uint32_t      tick;         // 上次电平变化时间
} Key_t;

// 定义四个按键
static Key_t keys[] = {
    {GPIOE,           KEY0_Pin,     GPIO_PIN_RESET, 0, 0},  // KEY0: 低有效
    {GPIOE,           KEY1_Pin,     GPIO_PIN_RESET, 0, 0},  // KEY1: 低有效
    {GPIOE,           KEY2_Pin,     GPIO_PIN_RESET, 0, 0},  // KEY2: 低有效
    {KEY_UP_GPIO_Port,KEY_UP_Pin,   GPIO_PIN_SET,   0, 0}   // KEY_UP: 高有效
};

#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))

#if 0
/**
 * @brief 旧版非阻塞按键扫描 (存在 Bug)
 * @note  原因分析：使用 uint8_t 变量 last_reported 存储单个按键位图。
 *        如果在多键按下或引脚噪声干扰下，last_reported 会被不同按键循环覆盖，
 *        导致 key != last_reported 的判断逻辑失效，变成“自动连点击”。
 */
KeyName_t Key_Scan_Old(void)
{
    static uint8_t last_reported = 0; // 记录上次返回的按键，避免重复触发
    uint8_t any_pressed = 0;
    KeyName_t triggered_key = KEY_NONE;
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < KEY_COUNT; i++)
    {
        Key_t* k = &keys[i];
        uint8_t raw = (HAL_GPIO_ReadPin(k->port, k->pin) == k->active_level) ? 1 : 0;

        if (raw != k->state)
        {
            // 电平变化，开始消抖计时
            k->tick = now;
            k->state = raw; // 暂存新状态（未确认）
        }
        else if (k->state == 1 && (now - k->tick) >= DEBOUNCE_TIME_MS)
        {
            // 确认按下（消抖完成）
            any_pressed = 1;
            if (last_reported != (1U << i))
            {
                triggered_key = (KeyName_t)i;
                last_reported = (1U << i);
            }
        }
    }

    if (!any_pressed)
    {
        last_reported = 0; // 所有键都释放了，清除记录
    }

    return triggered_key;
}
#endif

/**
 * @brief 修复版非阻塞按键扫描 (支持独立记录按键状态)
 * @note  解决方案：改用 Bitmask (位掩码) 来分别记录每一个按键的“已上报”状态。
 *        只有当物理状态为按下，且该位掩码中尚未标记“已上报”时，才产生一次触发。
 *        按键释放时，位掩码对应位清零。这样即使按住不放或存在引脚干扰，也不会连发。
 * @retval 被触发的按键枚举，无按键或已处理则返回 KEY_NONE
 */
KeyName_t Key_Scan(void)
{
    static uint8_t reported_mask = 0; // 独立位图记录，每个 bit 对应一个按键
    KeyName_t triggered_key = KEY_NONE;
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < KEY_COUNT; i++)
    {
        Key_t* k = &keys[i];
        uint8_t raw = (HAL_GPIO_ReadPin(k->port, k->pin) == k->active_level) ? 1 : 0;

        // 状态变化检测
        if (raw != k->state)
        {
            k->tick = now;
            k->state = raw; 
        }
        // 状态已稳定，检查是否按下
        else if (k->state == 1)
        {
            // 消抖时间达成
            if ((now - k->tick) >= DEBOUNCE_TIME_MS)
            {
                // 如果该位尚未上报，则触发单次事件
                if (!(reported_mask & (1 << i)))
                {
                    triggered_key = (KeyName_t)i;
                    reported_mask |= (1 << i); // 锁定上报状态
                }
            }
        }
        // 状态已稳定，检查是否释放
        else if (k->state == 0)
        {
            // 按键抬起，清除上报锁定标记
            reported_mask &= ~(1 << i);
        }
    }

    return triggered_key;
}