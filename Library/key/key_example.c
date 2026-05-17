#include "key.h"
#include "../../LED/LED.h"
#include "../../beep/beep.h"
#include "main.h"
/**
 * @brief 按键轮询使用案例
 * @note
 * - 在主循环中周期调用（建议每 5~10ms 调用一次）
 * - 调用前需先执行 `Key_Init()` 完成按键状态机初始化
 * - 本示例用于演示“按键事件 -> 外设动作”的业务映射
 */
void KEY_Example_Polling(void)
{
    /* 扫描一次按键，返回单次按下事件 */
    KeyName_t key = Key_Scan();

    /* KEY0 按下：翻转 LED0 状态 */
    if (key == KEY0) {
        LED_Toggle(LED0);
    }
    /* KEY1 按下：翻转 LED1 状态 */
    else if (key == KEY1) {
        LED_Toggle(LED1);
    }
    /* KEY_UP 按下：翻转蜂鸣器状态 */
    else if (key == KEY_UP) {
        BEEP_Toggle();
    }
    /* 其他按键或无事件：不处理 */
}
