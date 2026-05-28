#include "tftlcd.h"

/**
 * @brief LCD 底层驱动初始化示例
 * @details
 * 该示例用于确认以下几个最基础的链路是否已经打通：
 * 1. LCD 控制器初始化是否成功；
 * 2. 屏幕清屏是否正常；
 * 3. 坐标方向和逻辑分辨率是否正确。
 *
 * 验证方法：
 * - 执行本函数后，屏幕应先变为白色；
 * - 随后在四个角各出现一个红点；
 * - 如果四个角的位置正确，说明初始化、方向和画点功能正常。
 */
void TFTLCD_Example_BasicInit(void)
{
    TFTLCD_Init();

    /* 先将整屏清成白色，便于观察后续绘图结果。 */
    LCD_Clear(WHITE);

    /* 切换前景色为红色，在四个角绘制定位点。 */
    FRONT_COLOR = RED;
    LCD_DrawPoint(0U, 0U);
    LCD_DrawPoint((u16)(tftlcd_data.width - 1U), 0U);
    LCD_DrawPoint(0U, (u16)(tftlcd_data.height - 1U));
    LCD_DrawPoint((u16)(tftlcd_data.width - 1U),
                  (u16)(tftlcd_data.height - 1U));
}

/**
 * @brief LCD 窗口连续写入示例
 * @details
 * 该示例直接演示底层窗口写入流程：
 * 1. 将写窗口设置为 `(20,20)` 到 `(69,69)`；
 * 2. 向窗口连续写入 50x50 个蓝色像素；
 * 3. 最终在屏幕左上区域得到一个蓝色实心方块。
 *
 * 该示例适合验证：
 * - `LCD_Set_Window()` 设置范围是否正确；
 * - `LCD_WriteData_Color()` 连续写入是否正常；
 * - FSMC 写 GRAM 流程是否正常。
 */
void TFTLCD_Example_WindowFill(void)
{
    uint16_t x;
    uint16_t y;

    TFTLCD_Init();

    /* 先设置一个 50x50 的显示窗口。 */
    LCD_Set_Window(20U, 20U, 69U, 69U);

    /* 在窗口内连续写入蓝色像素。 */
    for (y = 0U; y < 50U; y++)
    {
        for (x = 0U; x < 50U; x++)
        {
            LCD_WriteData_Color(BLUE);
        }
    }
}

/**
 * @brief LCD 基础绘图示例
 * @details
 * 该示例在已经完成 LCD 初始化的基础上，依次测试：
 * 1. 区域填充；
 * 2. 画点；
 * 3. 画线；
 * 4. 画矩形；
 * 5. 画圆；
 * 6. 十字标记。
 *
 * 适合作为后续整理“基础绘图层”后的回归验证案例。
 */
void TFTLCD_Example_BasicDraw(void)
{
    TFTLCD_Init();

    /* 清空背景，避免旧图像干扰观察。 */
    LCD_Clear(WHITE);

    /* 测试单色区域填充。 */
    LCD_Fill(10U, 10U, 109U, 59U, YELLOW);
    LCD_Fill(120U, 10U, 219U, 59U, LIGHTBLUE);

    /* 测试画点和指定颜色画点。 */
    FRONT_COLOR = RED;
    LCD_DrawPoint(20U, 80U);
    LCD_DrawFRONT_COLOR(21U, 80U, BLUE);
    LCD_DrawFRONT_COLOR(22U, 80U, GREEN);

    /* 测试直线和带颜色直线。 */
    FRONT_COLOR = BLACK;
    LCD_DrawLine(10U, 100U, 150U, 100U);
    LCD_DrawLine_Color(10U, 110U, 150U, 150U, RED);

    /* 测试空心矩形和空心圆。 */
    FRONT_COLOR = MAGENTA;
    LCD_DrawRectangle(180U, 90U, 300U, 160U);
    FRONT_COLOR = BLUE;
    LCD_Draw_Circle(360U, 120U, 35U);

    /* 在圆心附近绘制一个十字标记，便于确认位置。 */
    LCD_DrowSign(360U, 120U, RED);
}
