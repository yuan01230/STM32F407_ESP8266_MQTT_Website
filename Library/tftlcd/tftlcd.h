#ifndef TFTLCD_H
#define TFTLCD_H

#include "main.h"
#include <stdint.h>

/*
 * LCD 驱动型号选择：
 * 根据实际屏幕控制器型号启用对应宏。
 * 当前工程默认使用 `HX8357DN`。
 */
//#define TFTLCD_HX8357D
//#define TFTLCD_HX8352C
//#define TFTLCD_ILI9341
//#define TFTLCD_ILI9327
//#define TFTLCD_ILI9486
//#define TFTLCD_R61509V
//#define TFTLCD_R61509VN
//#define TFTLCD_R61509V3
//#define TFTLCD_ST7793
//#define TFTLCD_NT35510
#define TFTLCD_HX8357DN
//#define TFTLCD_ILI9325
//#define TFTLCD_SSD1963
//#define TFTLCD_ILI9481
//#define TFTLCD_R61509VE
//#define TFTLCD_SSD1963N
//#define TFTLCD_ILI9488
//#define TFTLCD_ILI9806

/** 屏幕方向：0=竖屏，1=横屏。 */
#define TFTLCD_DIR 0

/** LCD 背光控制引脚。 */
#define LCD_LED PBout(15)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/**
 * @brief LCD FSMC 映射地址结构
 * @details
 * `LCD_CMD` 用于写命令寄存器，
 * `LCD_DATA` 用于写数据寄存器。
 */
typedef struct
{
    u16 LCD_CMD;
    u16 LCD_DATA;
} TFTLCD_TypeDef;

/*
 * FSMC/NOR-SRAM 地址映射说明：
 * 使用 Bank1 Sector4，A6 区分命令与数据区。
 */
#define TFTLCD_BASE ((u32)(0x6C000000 | 0x0000007E))
#define TFTLCD ((TFTLCD_TypeDef *)TFTLCD_BASE)

/**
 * @brief LCD 运行时关键参数
 */
typedef struct
{
    u16 width;
    u16 height;
    u16 id;
    u8 dir;
} _tftlcd_data;

/** 全局 LCD 参数对象。 */
extern _tftlcd_data tftlcd_data;
/** 当前前景色。 */
extern u16 FRONT_COLOR;
/** 当前背景色。 */
extern u16 BACK_COLOR;

/* 常用颜色定义，采用 RGB565 格式。 */
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

#ifdef TFTLCD_SSD1963
/* 以下参数仅在 SSD1963 RGB 屏配置下使用。 */
#define SSD_HOR_RESOLUTION 800
#define SSD_VER_RESOLUTION 480
#define SSD_HOR_PULSE_WIDTH 1
#define SSD_HOR_BACK_PORCH 210
#define SSD_HOR_FRONT_PORCH 45
#define SSD_VER_PULSE_WIDTH 1
#define SSD_VER_BACK_PORCH 34
#define SSD_VER_FRONT_PORCH 10
#define SSD_HT (SSD_HOR_RESOLUTION + SSD_HOR_PULSE_WIDTH + SSD_HOR_BACK_PORCH + SSD_HOR_FRONT_PORCH)
#define SSD_HPS (SSD_HOR_PULSE_WIDTH + SSD_HOR_BACK_PORCH)
#define SSD_VT (SSD_VER_PULSE_WIDTH + SSD_VER_BACK_PORCH + SSD_VER_FRONT_PORCH + SSD_VER_RESOLUTION)
#define SSD_VSP (SSD_VER_PULSE_WIDTH + SSD_VER_BACK_PORCH)
#endif

#ifdef TFTLCD_SSD1963N
/* 以下参数仅在 4.3 寸 SSD1963N RGB 屏配置下使用。 */
#define SSD_HOR_RESOLUTION 480
#define SSD_VER_RESOLUTION 272
#define SSD_HT 525
#define SSD_HPS 43
#define SSD_LPS 1
#define SSD_HPW 42
#define SSD_VDP 271
#define SSD_VT 288
#define SSD_VPS 12
#define SSD_FPS 1
#define SSD_VPW 11
#endif

/** 向 LCD 写命令。 */
void LCD_WriteCmd(u16 cmd);
/** 向 LCD 写数据。 */
void LCD_WriteData(u16 data);
/** 同时写命令和数据。 */
void LCD_WriteCmdData(u16 cmd, u16 data);
/** 连续写颜色数据。 */
void LCD_WriteData_Color(u16 color);

/** 初始化 LCD 控制器和显示参数。 */
void TFTLCD_Init(void);
/** 设置显存写入窗口。 */
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height);
/** 设置屏幕显示方向。 */
void LCD_Display_Dir(u8 dir);
/** 全屏清屏。 */
void LCD_Clear(u16 Color);
/** 清除指定矩形区域。 */
void LCD_ClearArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
/** 用单色填充指定矩形区域。 */
void LCD_Fill(u16 xState, u16 yState, u16 xEnd, u16 yEnd, u16 color);
/** 用颜色数组填充指定区域。 */
void LCD_Color_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 *color);
/** 按当前前景色绘制单个像素。 */
void LCD_DrawPoint(u16 x, u16 y);
/** 以指定颜色绘制单个像素。 */
void LCD_DrawFRONT_COLOR(u16 x, u16 y, u16 color);
/** 读取指定像素颜色。 */
u16 LCD_ReadPoint(u16 x, u16 y);
/** 绘制直线。 */
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);
/** 以指定颜色绘制直线。 */
void LCD_DrawLine_Color(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
/** 绘制十字标记。 */
void LCD_DrowSign(uint16_t x, uint16_t y, uint16_t color);
/** 绘制矩形框。 */
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
/** 绘制圆。 */
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r);
/** 显示单个 ASCII 字符。 */
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode);
/** 显示十进制整数。 */
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size);
/** 按指定模式显示整数。 */
void LCD_ShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode);
/** 显示字符串。 */
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u8 *p);
/** 混合显示 ASCII 与中文文本。 */
void LCD_ShowTextMixed(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *text);
/** 绘制单个中文字符。 */
void LCD_DrawOneChinese(uint16_t x, uint16_t y, const char *hz);
/** 显示中文字符串。 */
void LCD_ShowChinese(uint16_t x, uint16_t y, const char *hz);
/** 根据字模显示汉字。 */
void LCD_ShowFontHZ(u16 x, u16 y, u8 *cn);
/** 显示位图图片。 */
void LCD_ShowPicture(u16 x, u16 y, u16 wide, u16 high, u8 *pic);

#endif
