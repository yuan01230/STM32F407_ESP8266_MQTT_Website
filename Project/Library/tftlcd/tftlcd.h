#ifndef TFTLCD_H
#define TFTLCD_H

#include "main.h"
#include "tftlcd_port.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 当前 LCD 控制器类型标记
 * @note
 * 当前阶段仅整理 HX8357DN 底层驱动，不引入多控制器切换框架。
 * 保留该宏是为了兼容旧工程中可能存在的条件编译判断。
 */
#define TFTLCD_HX8357DN

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/**
 * @brief LCD 运行时参数
 * @details
 * 该结构体用于保存 LCD 当前的动态信息：
 * - `width` / `height`：当前显示方向下的逻辑分辨率
 * - `id`：LCD 控制器 ID
 * - `dir`：显示方向，`0` 表示竖屏，`1` 表示横屏
 */
typedef struct _tftlcd_data
{
    u16 width;
    u16 height;
    u16 id;
    u8 dir;
} _tftlcd_data;

extern _tftlcd_data tftlcd_data;
extern u16 FRONT_COLOR;
extern u16 BACK_COLOR;

#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0xF81F
#define GRED 0xFFE0
#define GBLUE 0x07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0xBC40
#define BRRED 0xFC07
#define GRAY 0x8430
#define DARKBLUE 0x01CF
#define LIGHTBLUE 0x7D7C
#define GRAYBLUE 0x5458
#define LIGHTGREEN 0x841F
#define LIGHTGRAY 0xEF5B
#define LGRAY 0xC618
#define LGRAYBLUE 0xA651
#define LBBLUE 0x2B12

void LCD_WriteCmd(u16 cmd);
void LCD_WriteData(u16 data);
void LCD_WriteCmdData(u16 cmd, u16 data);
void LCD_WriteData_Color(u16 color);

/**
 * @brief 初始化 LCD
 * @details
 * 调用流程：
 * 1. 端口层执行硬件复位并打开背光；
 * 2. 控制器层完成 HX8357DN 寄存器初始化；
 * 3. 设置默认显示方向；
 * 4. 清屏为白色。
 *
 * 使用前需要确保：
 * - `MX_GPIO_Init()` 已执行；
 * - `MX_FSMC_Init()` 已执行。
 */
void TFTLCD_Init(void);

/**
 * @brief 设置 LCD 写窗口
 * @param sx 起始 X 坐标
 * @param sy 起始 Y 坐标
 * @param width 结束 X 坐标
 * @param height 结束 Y 坐标
 * @note
 * 历史接口的参数名仍保留 `width/height`，但实际语义是结束坐标 `ex/ey`。
 * 为兼容现有上层调用，本次重构不修改函数签名。
 */
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height);

/**
 * @brief 设置 LCD 显示方向
 * @param dir `0` 竖屏，`1` 横屏
 */
void LCD_Display_Dir(u8 dir);

/**
 * @brief 全屏清屏
 * @param Color 填充颜色，RGB565
 */
void LCD_Clear(u16 Color);

/**
 * @brief 清除指定矩形区域
 * @param x0 左上角 X 坐标
 * @param y0 左上角 Y 坐标
 * @param x1 右下角 X 坐标
 * @param y1 右下角 Y 坐标
 * @note 当前实现使用白色填充该区域。
 */
void LCD_ClearArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief 用单色填充矩形区域
 * @param xState 起始 X 坐标
 * @param yState 起始 Y 坐标
 * @param xEnd 结束 X 坐标
 * @param yEnd 结束 Y 坐标
 * @param color 填充颜色，RGB565
 */
void LCD_Fill(u16 xState, u16 yState, u16 xEnd, u16 yEnd, u16 color);

/**
 * @brief 用颜色数组填充矩形区域
 * @param sx 起始 X 坐标
 * @param sy 起始 Y 坐标
 * @param ex 结束 X 坐标
 * @param ey 结束 Y 坐标
 * @param color 颜色数组指针
 */
void LCD_Color_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 *color);

/**
 * @brief 使用当前前景色绘制像素点
 * @param x 像素点 X 坐标
 * @param y 像素点 Y 坐标
 */
void LCD_DrawPoint(u16 x, u16 y);

/**
 * @brief 使用指定颜色绘制像素点
 * @param x 像素点 X 坐标
 * @param y 像素点 Y 坐标
 * @param color 像素点颜色，RGB565
 */
void LCD_DrawFRONT_COLOR(u16 x, u16 y, u16 color);

/**
 * @brief 读取指定像素点颜色
 * @param x 像素点 X 坐标
 * @param y 像素点 Y 坐标
 * @retval RGB565 颜色值，越界时返回 0
 */
u16 LCD_ReadPoint(u16 x, u16 y);

/**
 * @brief 使用当前前景色绘制直线
 * @param x1 起点 X 坐标
 * @param y1 起点 Y 坐标
 * @param x2 终点 X 坐标
 * @param y2 终点 Y 坐标
 */
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);

/**
 * @brief 使用指定颜色绘制直线
 * @param x1 起点 X 坐标
 * @param y1 起点 Y 坐标
 * @param x2 终点 X 坐标
 * @param y2 终点 Y 坐标
 * @param color 直线颜色，RGB565
 */
void LCD_DrawLine_Color(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);

/**
 * @brief 绘制十字标记
 * @param x 标记中心 X 坐标
 * @param y 标记中心 Y 坐标
 * @param color 标记颜色，RGB565
 */
void LCD_DrowSign(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 使用当前前景色绘制空心矩形
 * @param x1 第一个对角点 X 坐标
 * @param y1 第一个对角点 Y 坐标
 * @param x2 第二个对角点 X 坐标
 * @param y2 第二个对角点 Y 坐标
 */
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);

/**
 * @brief 使用当前前景色绘制空心圆
 * @param x0 圆心 X 坐标
 * @param y0 圆心 Y 坐标
 * @param r 圆半径
 */
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r);

/**
 * @brief 显示单个 ASCII 字符
 * @param x 字符左上角 X 坐标
 * @param y 字符左上角 Y 坐标
 * @param num 要显示的字符，范围为 `' '` 到 `'~'`
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param mode 显示模式，`0` 为非叠加，`1` 为叠加
 */
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode);

/**
 * @brief 显示十进制无符号整数，高位 0 不显示
 * @param x 数字左上角起始 X 坐标
 * @param y 数字左上角起始 Y 坐标
 * @param num 要显示的数值
 * @param len 总显示位数
 * @param size 字体高度，可选 `12`、`16`、`24`
 */
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size);

/**
 * @brief 显示十进制无符号整数，可配置前导位填充方式
 * @param x 数字左上角起始 X 坐标
 * @param y 数字左上角起始 Y 坐标
 * @param num 要显示的数值
 * @param len 总显示位数
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param mode `bit7` 控制前导 0 填充，`bit0` 控制叠加显示
 */
void LCD_ShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode);

/**
 * @brief 在指定区域内显示 ASCII 字符串
 * @param x 显示区域左上角 X 坐标
 * @param y 显示区域左上角 Y 坐标
 * @param width 显示区域宽度
 * @param height 显示区域高度
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param p 字符串首地址
 */
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u8 *p);
void LCD_ShowTextMixed(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *text);
void LCD_ShowTextUtf8(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *utf8);
void LCD_DrawOneChinese(uint16_t x, uint16_t y, const char *hz);
void LCD_ShowChinese(uint16_t x, uint16_t y, const char *hz);
void LCD_ShowFontHZ(u16 x, u16 y, u8 *cn);
void LCD_ShowPicture(u16 x, u16 y, u16 wide, u16 high, u8 *pic);

#ifdef __cplusplus
}
#endif

#endif
