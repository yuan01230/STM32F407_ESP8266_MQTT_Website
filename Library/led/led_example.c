#include "led.h"
#include "../../delay/delay.h"

/**
 * @brief LED 基础使用案例（单次执行）
 * @note
 * - 该示例用于演示 LED 驱动最基本的开关流程
 * - 调用前需确保 `MX_GPIO_Init()` 已执行
 * - 依赖 delay 驱动提供阻塞延时
 */
void LED_Example_RunOnce(void)
{
    /* 第一步：初始化 LED 驱动，默认关闭全部 LED */
    LED_Init();

    /* 第二步：点亮 LED0，保持 200ms 后关闭 */
    LED_On(LED0);
    delay_ms(200);
    LED_Off(LED0);

    /* 第三步：点亮 LED1，保持 200ms 后关闭 */
    LED_On(LED1);
    delay_ms(200);
    LED_Off(LED1);
}
