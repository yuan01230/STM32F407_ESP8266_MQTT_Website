#include "tftlcd_hx8357dn.h"

#include "tftlcd.h"
#include "tftlcd_port.h"

/**
 * @brief 向 HX8357DN 写命令
 * @details
 * LCD 采用 FSMC 8080 总线方式访问时，写命令本质上就是向
 * 命令映射地址写入一个 16 位值。
 */
void HX8357DN_WriteCmd(uint16_t cmd)
{
    TFTLCD_PORT->LCD_CMD = cmd;
}

/**
 * @brief 向 HX8357DN 写数据
 */
void HX8357DN_WriteData(uint16_t data)
{
    TFTLCD_PORT->LCD_DATA = data;
}

/**
 * @brief 从 HX8357DN 读数据
 */
uint16_t HX8357DN_ReadData(void)
{
    return TFTLCD_PORT->LCD_DATA;
}

/**
 * @brief 向 LCD GRAM 写入一个 RGB565 像素
 * @details
 * 当前工程验证通过的访问方式中，HX8357DN 在这个总线连接模式下，
 * 需要按照“高 8 位 + 低 8 位”顺序连续写入，才能正确落到显存。
 * 这个细节由控制器层封装后，上层绘图代码无需关心。
 */
void HX8357DN_WriteColor(uint16_t color)
{
    TFTLCD_PORT->LCD_DATA = (uint16_t)(color >> 8);
    TFTLCD_PORT->LCD_DATA = (uint16_t)(color & 0x00FFU);
}

/**
 * @brief 设置 LCD 显示方向并同步更新分辨率参数
 * @details
 * 该函数除了写入 `0x36` 方向寄存器，还会把当前逻辑宽高同步
 * 保存到 `runtime`，这样上层绘图接口能统一使用 `tftlcd_data`
 * 来做边界判断与窗口计算。
 */
void HX8357DN_SetDisplayDir(struct _tftlcd_data *runtime, uint8_t dir)
{
    runtime->dir = dir;

    HX8357DN_WriteCmd(0x36);
    if (dir == 0U)
    {
        HX8357DN_WriteData(0x4C);
        runtime->width = 320U;
        runtime->height = 480U;
    }
    else
    {
        HX8357DN_WriteData(0x2C);
        runtime->width = 480U;
        runtime->height = 320U;
    }
}

/**
 * @brief 设置 HX8357DN 的 GRAM 写窗口
 * @details
 * 设置流程：
 * 1. `0x2A`：设置列地址范围
 * 2. `0x2B`：设置页地址范围
 * 3. `0x2C`：进入显存写入状态
 *
 * 注意：
 * - `ex/ey` 表示结束坐标，不是宽高
 * - 公共层保留旧接口参数名，是为了兼容原有代码
 */
void HX8357DN_SetWindow(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    HX8357DN_WriteCmd(0x2A);
    HX8357DN_WriteData((uint16_t)(sx >> 8));
    HX8357DN_WriteData((uint16_t)(sx & 0x00FFU));
    HX8357DN_WriteData((uint16_t)(ex >> 8));
    HX8357DN_WriteData((uint16_t)(ex & 0x00FFU));

    HX8357DN_WriteCmd(0x2B);
    HX8357DN_WriteData((uint16_t)(sy >> 8));
    HX8357DN_WriteData((uint16_t)(sy & 0x00FFU));
    HX8357DN_WriteData((uint16_t)(ey >> 8));
    HX8357DN_WriteData((uint16_t)(ey & 0x00FFU));

    HX8357DN_WriteCmd(0x2C);
}

/**
 * @brief 读取控制器 ID
 * @details
 * 当前工程沿用原有驱动方式，通过读取 `0xD0` 寄存器获取 ID。
 * 本次重构只做职责拆分，不改动原有时序和寄存器访问流程。
 */
uint16_t HX8357DN_ReadId(void)
{
    uint16_t id;

    HX8357DN_WriteCmd(0xD0);
    (void)HX8357DN_ReadData();
    id = HX8357DN_ReadData();

    return id;
}

/**
 * @brief 初始化 HX8357DN 控制器
 * @details
 * 这里保留原工程已经验证可工作的初始化序列，不擅自修改寄存器值。
 * 这样可以把本次改动风险控制在“代码组织重构”层面，而不是同时引入
 * “寄存器参数变化”这个额外变量。
 */
void HX8357DN_Init(struct _tftlcd_data *runtime)
{
    runtime->id = HX8357DN_ReadId();

    HX8357DN_WriteCmd(0xE9);
    HX8357DN_WriteData(0x20);

    HX8357DN_WriteCmd(0x11);
    HAL_Delay(120);

    HX8357DN_WriteCmd(0x3A);
    HX8357DN_WriteData(0x55);

    HX8357DN_WriteCmd(0xD1);
    HX8357DN_WriteData(0x00);
    HX8357DN_WriteData(0x65);
    HX8357DN_WriteData(0x1F);

    HX8357DN_WriteCmd(0xD0);
    HX8357DN_WriteData(0x07);
    HX8357DN_WriteData(0x07);
    HX8357DN_WriteData(0x80);

    HX8357DN_WriteCmd(0x36);
    HX8357DN_WriteData(0x4C);

    HX8357DN_WriteCmd(0xC1);
    HX8357DN_WriteData(0x10);
    HX8357DN_WriteData(0x10);
    HX8357DN_WriteData(0x02);
    HX8357DN_WriteData(0x02);

    HX8357DN_WriteCmd(0xC0);
    HX8357DN_WriteData(0x00);
    HX8357DN_WriteData(0x35);
    HX8357DN_WriteData(0x00);
    HX8357DN_WriteData(0x00);
    HX8357DN_WriteData(0x01);
    HX8357DN_WriteData(0x02);

    HX8357DN_WriteCmd(0xC4);
    HX8357DN_WriteData(0x03);

    HX8357DN_WriteCmd(0xC5);
    HX8357DN_WriteData(0x01);

    HX8357DN_WriteCmd(0xD2);
    HX8357DN_WriteData(0x01);
    HX8357DN_WriteData(0x22);

    HX8357DN_WriteCmd(0xE7);
    HX8357DN_WriteData(0x38);

    HX8357DN_WriteCmd(0xF3);
    HX8357DN_WriteData(0x08);
    HX8357DN_WriteData(0x12);
    HX8357DN_WriteData(0x12);
    HX8357DN_WriteData(0x08);

    HX8357DN_WriteCmd(0xC8);
    HX8357DN_WriteData(0x01);
    HX8357DN_WriteData(0x52);
    HX8357DN_WriteData(0x37);
    HX8357DN_WriteData(0x10);
    HX8357DN_WriteData(0x0D);
    HX8357DN_WriteData(0x01);
    HX8357DN_WriteData(0x04);
    HX8357DN_WriteData(0x51);
    HX8357DN_WriteData(0x77);
    HX8357DN_WriteData(0x01);
    HX8357DN_WriteData(0x01);
    HX8357DN_WriteData(0x0D);
    HX8357DN_WriteData(0x08);
    HX8357DN_WriteData(0x80);
    HX8357DN_WriteData(0x00);

    HX8357DN_WriteCmd(0x29);
}

/**
 * @brief 读取指定像素点的颜色值
 * @details
 * HX8357DN 读显存时不能直接得到目标像素颜色，一般流程是：
 * 1. 先设置读窗口到目标点
 * 2. 发送 `0x2E` 读显存命令
 * 3. 丢弃前面的过渡读值
 * 4. 组合有效字节恢复成 RGB565 颜色
 */
uint16_t HX8357DN_ReadPoint(uint16_t x, uint16_t y)
{
    uint16_t high;
    uint16_t low;

    HX8357DN_SetWindow(x, y, x, y);
    HX8357DN_WriteCmd(0x2E);

    (void)TFTLCD_PORT->LCD_DATA;
    high = TFTLCD_PORT->LCD_DATA;
    low = TFTLCD_PORT->LCD_DATA;
    (void)TFTLCD_PORT->LCD_DATA;

    return (uint16_t)((high << 8) | (low & 0x00FFU));
}
