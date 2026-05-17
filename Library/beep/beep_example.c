#include "beep.h"
#include "../../delay/delay.h"

/**
 * @brief 蜂鸣器鸣叫 100ms 示例
 * @note
 * - 演示蜂鸣器驱动最小调用链路：初始化 -> 打开 -> 延时 -> 关闭
 * - 调用前应确保 `MX_GPIO_Init()` 已执行
 */
void BEEP_Example_Beep100ms(void)
{
    /* 初始化蜂鸣器驱动，默认关闭 */
    BEEP_Init();

    /* 打开蜂鸣器并保持 100ms */
    BEEP_On();
    delay_ms(100);

    /* 到时后关闭蜂鸣器 */
    BEEP_Off();
}
