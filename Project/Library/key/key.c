#include "key.h"
#include "gpio_device.h"
#include "board_drv_config.h"

/*
 * 按键驱动设计说明：
 * 1) 每个按键维护独立状态机（稳定状态/上报标志/边沿时间戳）
 * 2) 使用固定消抖时间过滤机械抖动
 * 3) 仅在“按下沿”上报一次事件，避免长按重复触发
 * 4) 驱动只产生“按键事件”，不直接操作业务外设，保持模块解耦
 */
typedef struct {
    uint8_t stable_pressed; /**< 当前稳定状态：1=按下，0=释放 */
    uint8_t reported;       /**< 当前按下状态是否已上报 */
    uint32_t edge_tick;     /**< 最近一次状态变化计时参考（ms） */
} key_runtime_t;

/*
 * 板级按键静态配置表：
 * - 下标 0/1/2/3 对应 KEY0/KEY1/KEY2/KEY_UP
 * - 每项包含 GPIO 端口、引脚、有效电平信息
 */
static const gpio_device_t s_key_table[] = {
    BOARD_KEY_CONFIG_TABLE
};

/*
 * 按键运行时状态表：
 * - 与 s_key_table 按下标一一对应
 * - 仅保存“运行过程中变化”的状态，不保存硬件配置
 */
static key_runtime_t s_key_rt[BOARD_KEY_COUNT];

/**
 * @brief 初始化按键运行时状态机
 * @note
 * - 读取当前按键电平作为初始稳定状态，避免上电后立即误触发
 * - 建议在 `MX_GPIO_Init()` 后、主循环前调用一次
 */
void Key_Init(void)
{
    for (uint32_t i = 0; i < BOARD_KEY_COUNT; ++i) {
        s_key_rt[i].stable_pressed = gpio_device_read(&s_key_table[i]);
        s_key_rt[i].reported = 0U;
        s_key_rt[i].edge_tick = HAL_GetTick();
    }
}

/**
 * @brief 扫描按键并返回单次按下事件
 * @return KeyName_t
 * - 返回 KEY0/KEY1/KEY2/KEY_UP：表示本次扫描检测到对应按键“新按下”
 * - 返回 KEY_NONE：表示无新按键事件
 *
 * @note 工作流程（单键）：
 * 1. 读取原始状态 raw_pressed（已统一为逻辑状态）
 * 2. 若 raw 与 stable 不同：进入消抖确认阶段
 * 3. 变化持续时间超过 KEY_DEFAULT_DEBOUNCE_MS 后才更新 stable
 * 4. stable=按下 且未上报 -> 上报一次并锁定
 * 5. stable=释放 -> 清除锁定，允许下次按下再次上报
 */
KeyName_t Key_Scan(void)
{
    const uint32_t now = HAL_GetTick();

    for (uint32_t i = 0; i < BOARD_KEY_COUNT; ++i) {
        key_runtime_t *rt = &s_key_rt[i];
        const uint8_t raw_pressed = gpio_device_read(&s_key_table[i]);

        /*
         * 场景1：采样状态与稳定状态不一致
         * 含义：按键可能刚变化，也可能是抖动；先进入消抖观察
         */
        if (raw_pressed != rt->stable_pressed) {
            /*
             * 仅当变化持续超过消抖窗口才确认状态切换，抑制机械抖动误判
             */
            if ((now - rt->edge_tick) >= KEY_DEFAULT_DEBOUNCE_MS) {
                rt->stable_pressed = raw_pressed;
                rt->edge_tick = now;

                /*
                 * 一旦确认按键已释放，必须清除 reported 锁
                 * 否则下次按下将不会再次触发事件
                 */
                if (rt->stable_pressed == 0U) {
                    rt->reported = 0U;
                }
            }
            continue;
        }

        /*
         * 场景2：采样状态与稳定状态一致
         * 含义：当前处于稳定区间，更新时间戳作为下一次变化的起点
         */
        rt->edge_tick = now;

        /*
         * 场景3：稳定按下且尚未上报
         * 触发一次事件后立刻置 reported=1，防止长按期间重复触发
         */
        if (rt->stable_pressed == 1U && rt->reported == 0U) {
            rt->reported = 1U;
            return (KeyName_t)i;
        }

        /*
         * 场景4：稳定释放
         * 兜底清理 reported，确保下一次按下可触发
         */
        if (rt->stable_pressed == 0U) {
            rt->reported = 0U;
        }
    }

    return KEY_NONE;
}
