//
// Created by 鏍囪 on 25-12-30.
//

/**
 ******************************************************************************
 * @file    tftlcd.c
 * @brief   HX8357DN TFT LCD driver (HAL + FSMC)
 ******************************************************************************
 */

/*
 * 说明：
 * - 本文件是 LCD 底层驱动和基础绘图实现。
 * - 上层 UI、图片模块、字库显示最终都会落到这里的基础接口。
 * - 当前职责可分为三类：
 *   1. LCD 控制器初始化与方向配置；
 *   2. 像素、直线、矩形、清屏等基础绘图；
 *   3. ASCII、中文、混排文本和位图显示。
 */

#include "tftlcd.h"
#include "tftlcd_hx8357dn.h"
#include "stdlib.h"
#include "font.h"
#include "string.h"
#include "ff.h"
#include "../font_storage/font_storage.h"
#include "../font_codec/font_codec.h"

//LCD鐨勭敾绗旈鑹插拰鑳屾櫙鑹?
u16 FRONT_COLOR=BLACK;	//鐢荤瑪棰滆壊
u16 BACK_COLOR=WHITE;  //鑳屾櫙鑹?
_tftlcd_data tftlcd_data;

/**
 * @brief 判断指定坐标是否位于当前 LCD 逻辑显示区域内
 * @param x 像素点 X 坐标
 * @param y 像素点 Y 坐标
 * @retval 1 坐标有效
 * @retval 0 坐标越界
 */
static uint8_t LCD_IsPointValid(u16 x, u16 y)
{
    if ((x >= tftlcd_data.width) || (y >= tftlcd_data.height))
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 对矩形区域参数进行排序与裁剪
 * @param x0 左上角 X 坐标指针
 * @param y0 左上角 Y 坐标指针
 * @param x1 右下角 X 坐标指针
 * @param y1 右下角 Y 坐标指针
 * @retval 1 区域有效，可继续绘制
 * @retval 0 区域无效，无需绘制
 * @details
 * 该辅助函数统一处理起止坐标传反、坐标越界和空区域问题，
 * 便于基础绘图函数复用同一套边界保护逻辑。
 */
static uint8_t LCD_NormalizeRect(u16 *x0, u16 *y0, u16 *x1, u16 *y1)
{
    u16 temp;

    if (*x0 > *x1)
    {
        temp = *x0;
        *x0 = *x1;
        *x1 = temp;
    }

    if (*y0 > *y1)
    {
        temp = *y0;
        *y0 = *y1;
        *y1 = temp;
    }

    if ((*x0 >= tftlcd_data.width) || (*y0 >= tftlcd_data.height))
    {
        return 0U;
    }

    if (*x1 >= tftlcd_data.width)
    {
        *x1 = (u16)(tftlcd_data.width - 1U);
    }

    if (*y1 >= tftlcd_data.height)
    {
        *y1 = (u16)(tftlcd_data.height - 1U);
    }

    return 1U;
}

/**
 * @brief 向已经设置好的矩形窗口连续写入同一种颜色
 * @param width 窗口宽度，单位：像素
 * @param height 窗口高度，单位：像素
 * @param color 要写入的 RGB565 颜色
 */
static void LCD_WriteSolidColor(uint16_t width, uint16_t height, u16 color)
{
    uint32_t pixel_count;

    pixel_count = (uint32_t)width * (uint32_t)height;
    while (pixel_count > 0U)
    {
        LCD_WriteData_Color(color);
        pixel_count--;
    }
}

/**
 * @brief 获取 ASCII 字符宽度
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @retval 字符宽度，单位：像素
 * @retval 0 不支持的字体尺寸
 * @details
 * 当前工程中的 ASCII 字模采用“半宽字符”组织方式，
 * 因此字符宽度恒等于 `size / 2`。
 */
static uint8_t LCD_GetAsciiCharWidth(u8 size)
{
    if ((size != 12U) && (size != 16U) && (size != 24U))
    {
        return 0U;
    }

    return (uint8_t)(size / 2U);
}

/**
 * @brief 获取 ASCII 字模数据指针
 * @param ch 要显示的 ASCII 字符
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param glyph_bytes 输出参数，返回单个字符占用的字节数
 * @retval 成功时返回字模首地址
 * @retval 失败时返回 `NULL`
 * @details
 * 当前字库从空格 `' '` 开始连续存放到 `~`，因此访问前需要先做
 * 可显示 ASCII 范围判断，再通过偏移量取得对应字模。
 */
static const uint8_t *LCD_GetAsciiGlyph(u8 ch, u8 size, u8 *glyph_bytes)
{
    uint8_t index;

    if (glyph_bytes == NULL)
    {
        return NULL;
    }

    if ((ch < (u8)' ') || (ch > (u8)'~'))
    {
        return NULL;
    }

    index = (uint8_t)(ch - (u8)' ');
    if (size == 12U)
    {
        *glyph_bytes = 12U;
        return ascii_1206[index];
    }
    if (size == 16U)
    {
        *glyph_bytes = 16U;
        return ascii_1608[index];
    }
    if (size == 24U)
    {
        *glyph_bytes = 36U;
        return ascii_2412[index];
    }

    return NULL;
}

/**
 * @brief 向 LCD 写入命令
 * @details
 * 对业务层暴露稳定 API，内部具体访问细节已经下沉到
 * `tftlcd_hx8357dn.c` 控制器层，便于后续维护。
 */
void LCD_WriteCmd(u16 cmd)
{
    HX8357DN_WriteCmd(cmd);
}

/**
 * @brief 向 LCD 写入数据
 */
void LCD_WriteData(u16 data)
{
    HX8357DN_WriteData(data);
}

/**
 * @brief 连续写入一个命令和对应数据
 */
void LCD_WriteCmdData(u16 cmd,u16 data)
{
	LCD_WriteCmd(cmd);
	LCD_WriteData(data);
}

/**
 * @brief 向当前 GRAM 窗口写入一个 RGB565 像素
 * @details
 * 上层绘图函数只关心“写入一个像素颜色”，不需要关心底层总线
 * 是直接写 16 位还是拆分成高低字节，这些差异由控制器层统一处理。
 */
void LCD_WriteData_Color(u16 color)
{
    HX8357DN_WriteColor(color);
}

/**
 * @brief 设置 LCD 显示方向
 * @param dir `0` 竖屏，`1` 横屏
 * @details
 * 除了更新控制器寄存器外，还会同步更新 `tftlcd_data.width/height`，
 * 这样上层绘图函数始终基于当前方向下的逻辑分辨率工作。
 */
void LCD_Display_Dir(u8 dir)
{
    HX8357DN_SetDisplayDir(&tftlcd_data, dir);
}

/**
 * @brief 设置 LCD 显示窗口
 * @param sx 起始 X 坐标
 * @param sy 起始 Y 坐标
 * @param width 结束 X 坐标
 * @param height 结束 Y 坐标
 * @note
 * 历史接口的参数名仍保留 `width/height`，但实际语义是结束坐标 `ex/ey`。
 * 当前阶段为了兼容现有工程调用点，不调整接口名称与参数名。
 */
void LCD_Set_Window(u16 sx,u16 sy,u16 width,u16 height)
{
    HX8357DN_SetWindow(sx, sy, width, height);
}

/**
 * @brief 读取指定像素点的颜色值
 */
u16 LCD_ReadPoint(u16 x,u16 y)
{
	if(x>=tftlcd_data.width||y>=tftlcd_data.height)return 0;
    return HX8357DN_ReadPoint(x, y);
}



//SSD1963 鑳屽厜璁剧疆
//pwm:鑳屽厜绛夌骇,0~100.瓒婂ぇ瓒婁寒.
void LCD_SSD_BackLightSet(u8 pwm)
{
	LCD_WriteCmd(0xBE);	//閰嶇疆PWM杈撳嚭
	LCD_WriteData(0x05);	//1璁剧疆PWM棰戠巼
	LCD_WriteData(pwm*2.55);//2璁剧疆PWM鍗犵┖姣?	LCD_WriteData(0x01);	//3璁剧疆C
	LCD_WriteData(0xFF);	//4璁剧疆D
	LCD_WriteData(0x00);	//5璁剧疆E
	LCD_WriteData(0x00);	//6璁剧疆F
}

void TFTLCD_Init(void)
{
    /*
     * 初始化顺序说明：
     * 1. 端口层执行硬件复位并打开背光
     * 2. 控制器层完成 HX8357DN 寄存器初始化
     * 3. 设置默认显示方向
     * 4. 清屏为白色，给上层一个确定的初始状态
     */
    TFTLCD_Port_Init();
    HX8357DN_Init(&tftlcd_data);
	LCD_Display_Dir(TFTLCD_DEFAULT_DIR);
	LCD_Clear(WHITE);
}

/**
 * @brief 全屏清屏
 * @param color 填充颜色，RGB565
 * @details
 * 当前实现通过设置整屏窗口后，逐像素连续写入颜色值。
 * 这是最基础也最稳定的清屏方式，上层很多页面切换都依赖它。
 */
void LCD_Clear(u16 color)
{
	LCD_Set_Window(0U, 0U,
                   (u16)(tftlcd_data.width - 1U),
                   (u16)(tftlcd_data.height - 1U));
    LCD_WriteSolidColor(tftlcd_data.width, tftlcd_data.height, color);
}

/**
 * @brief 清除指定矩形区域
 * @param x0 左上角 X
 * @param y0 左上角 Y
 * @param x1 右下角 X
 * @param y1 右下角 Y
 * @details
 * 与全屏清屏相比，这个接口更适合 UI 局部刷新。
 * 例如图片预览区清背景、列表页单行重绘前擦除旧内容，都会优先使用它。
 */
void LCD_ClearArea(uint16_t x0, uint16_t y0,
                   uint16_t x1, uint16_t y1)
{
    uint16_t width, height;

    if (LCD_NormalizeRect(&x0, &y0, &x1, &y1) == 0U)
    {
        return;
    }

    width = (uint16_t)(x1 - x0 + 1U);
    height = (uint16_t)(y1 - y0 + 1U);

    LCD_Set_Window(x0, y0, x1, y1);
    LCD_WriteSolidColor(width, height, WHITE);
}

/**
 * @brief 用单色填充矩形区域
 * @details
 * 这是 LCD 层最常用的基础绘图接口之一。
 * 上层页面会把它当作：
 * - 清某个小区域
 * - 绘制底栏背景
 * - 绘制分隔区块
 * 来使用。
 */
void LCD_Fill(u16 xState,u16 yState,u16 xEnd,u16 yEnd,u16 color)
{
    uint16_t width;
    uint16_t height;

    if (LCD_NormalizeRect(&xState, &yState, &xEnd, &yEnd) == 0U)
    {
        return;
    }

    width = (uint16_t)(xEnd - xState + 1U);
    height = (uint16_t)(yEnd - yState + 1U);

	LCD_Set_Window(xState, yState, xEnd, yEnd);
    LCD_WriteSolidColor(width, height, color);
}

/**
 * @brief 用颜色数组填充矩形区域
 * @param sx 起始 X 坐标
 * @param sy 起始 Y 坐标
 * @param ex 结束 X 坐标
 * @param ey 结束 Y 坐标
 * @param color 指向颜色数组的指针
 * @details
 * `color` 数组中的颜色数据应按从左到右、从上到下的顺序排列。
 * 适合显示图标缓存、软件生成图像或者小尺寸位图。
 */
void LCD_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color)
{
	u16 height;
	u16 width;
	uint32_t index;

    if (color == NULL)
    {
        return;
    }

    if (LCD_NormalizeRect(&sx, &sy, &ex, &ey) == 0U)
    {
        return;
    }

	width = (u16)(ex - sx + 1U);
	height = (u16)(ey - sy + 1U);

    LCD_Set_Window(sx, sy, ex, ey);
    for (index = 0U; index < ((uint32_t)width * (uint32_t)height); index++)
    {
        LCD_WriteData_Color(color[index]);
    }
}

/**
 * @brief 使用当前前景色绘制单个像素点
 * @param x 像素点 X 坐标
 * @param y 像素点 Y 坐标
 */
void LCD_DrawPoint(u16 x,u16 y)
{
    if (LCD_IsPointValid(x, y) == 0U)
    {
        return;
    }

	LCD_Set_Window(x, y, x, y);
	LCD_WriteData_Color(FRONT_COLOR);
}

/**
 * @brief 使用指定颜色绘制单个像素点
 * @param x 像素点 X 坐标
 * @param y 像素点 Y 坐标
 * @param color 像素点颜色，RGB565
 */
void LCD_DrawFRONT_COLOR(u16 x,u16 y,u16 color)
{
    if (LCD_IsPointValid(x, y) == 0U)
    {
        return;
    }

	LCD_Set_Window(x, y, x, y);
	LCD_WriteData_Color(color);
}

/**
 * @brief 使用当前前景色绘制一条直线
 * @param x1 起点 X 坐标
 * @param y1 起点 Y 坐标
 * @param x2 终点 X 坐标
 * @param y2 终点 Y 坐标
 * @details
 * 采用 Bresenham 直线算法，只使用整数运算，
 * 适合 MCU 环境下的基础图元绘制。
 */
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
	u16 t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //璁＄畻鍧愭爣澧為噺
	delta_y=y2-y1;
	uRow=x1;
	uCol=y1;
	if(delta_x>0)incx=1; //璁剧疆鍗曟鏂瑰悜
	else if(delta_x==0)incx=0;//鍨傜洿绾?	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if(delta_y==0)incy=0;//姘村钩绾?	else{incy=-1;delta_y=-delta_y;}
	if( delta_x>delta_y)distance=delta_x; //閫夊彇鍩烘湰澧為噺鍧愭爣杞?	else distance=delta_y;
	for(t=0;t<=distance+1;t++ )//鐢荤嚎杈撳嚭
	{
		LCD_DrawPoint(uRow,uCol);//鐢荤偣
		xerr+=delta_x ;
		yerr+=delta_y ;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}

/**
 * @brief 使用指定颜色绘制一条直线
 * @param x1 起点 X 坐标
 * @param y1 起点 Y 坐标
 * @param x2 终点 X 坐标
 * @param y2 终点 Y 坐标
 * @param color 直线颜色，RGB565
 */
void LCD_DrawLine_Color(u16 x1, u16 y1, u16 x2, u16 y2,u16 color)
{
	u16 t;
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //璁＄畻鍧愭爣澧為噺
	delta_y=y2-y1;
	uRow=x1;
	uCol=y1;
	if(delta_x>0)incx=1; //璁剧疆鍗曟鏂瑰悜
	else if(delta_x==0)incx=0;//鍨傜洿绾?	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if(delta_y==0)incy=0;//姘村钩绾?	else{incy=-1;delta_y=-delta_y;}
	if( delta_x>delta_y)distance=delta_x; //閫夊彇鍩烘湰澧為噺鍧愭爣杞?	else distance=delta_y;
	for(t=0;t<=distance+1;t++ )//鐢荤嚎杈撳嚭
	{
		LCD_DrawFRONT_COLOR(uRow,uCol,color);//鐢荤偣
		xerr+=delta_x ;
		yerr+=delta_y ;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}


/**
 * @brief 在指定坐标绘制十字标记
 * @param x 标记中心 X 坐标
 * @param y 标记中心 Y 坐标
 * @param color 标记颜色，RGB565
 * @details
 * 默认绘制一个 9x9 十字标记，中心为 3x3 色块，
 * 常用于触摸调试、坐标校验或关键点标注。
 */
void LCD_DrowSign(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t i;

    if ((x < 4U) || (y < 4U))
    {
        return;
    }

    if (((x + 4U) >= tftlcd_data.width) || ((y + 4U) >= tftlcd_data.height))
    {
        return;
    }

    /* 绘制中心 3x3 色块 */
    LCD_Set_Window(x-1, y-1, x+1, y+1);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);
    }

    /* 绘制横向笔画 */
    LCD_Set_Window(x-4, y, x+4, y);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);
    }

    /* 绘制纵向笔画 */
    LCD_Set_Window(x, y-4, x, y+4);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);
    }
}

/**
 * @brief 使用当前前景色绘制空心矩形
 * @param x1 第一个对角点 X 坐标
 * @param y1 第一个对角点 Y 坐标
 * @param x2 第二个对角点 X 坐标
 * @param y2 第二个对角点 Y 坐标
 */
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}

/**
 * @brief 使用当前前景色绘制空心圆
 * @param x0 圆心 X 坐标
 * @param y0 圆心 Y 坐标
 * @param r 圆半径，单位：像素
 * @details
 * 采用 Bresenham 中点画圆算法，通过八分对称关系输出圆周像素点。
 */
void LCD_Draw_Circle(u16 x0,u16 y0,u8 r)
{
	int a,b;
	int di;
	a=0;b=r;
	di=3-(r<<1);             //鍒ゆ柇涓嬩釜鐐逛綅缃殑鏍囧織
	while(a<=b)
	{
		LCD_DrawPoint(x0+a,y0-b);             //5
 		LCD_DrawPoint(x0+b,y0-a);             //0
		LCD_DrawPoint(x0+b,y0+a);             //4
		LCD_DrawPoint(x0+a,y0+b);             //6
		LCD_DrawPoint(x0-a,y0+b);             //1
 		LCD_DrawPoint(x0-b,y0+a);
		LCD_DrawPoint(x0-a,y0-b);             //2
  		LCD_DrawPoint(x0-b,y0-a);             //7
		a++;
		//浣跨敤Bresenham绠楁硶鐢诲渾
		if(di<0)di +=4*a+6;
		else
		{
			di+=10+4*(a-b);
			b--;
		}
	}
}



/**
 * @brief 显示单个 ASCII 字符
 * @param x 字符左上角 X 坐标
 * @param y 字符左上角 Y 坐标
 * @param num 要显示的字符，范围为 `' '` 到 `'~'`
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param mode 显示模式，`0` 为非叠加，`1` 为叠加
 * @details
 * - 非叠加模式：字模位为 0 时会同时绘制背景色，适合普通文字刷新。
 * - 叠加模式：字模位为 0 时不处理背景，适合在已有图形上覆盖显示。
 */
void LCD_ShowChar(u16 x,u16 y,u8 num,u8 size,u8 mode)
{
    u8 temp;
    u8 bit_index;
    u8 glyph_index;
    u8 glyph_bytes;
    u8 char_width;
    u16 origin_y;
    const uint8_t *glyph;

    char_width = LCD_GetAsciiCharWidth(size);
    glyph = LCD_GetAsciiGlyph(num, size, &glyph_bytes);
    if ((char_width == 0U) || (glyph == NULL))
    {
        return;
    }

    origin_y = y;
    for (glyph_index = 0U; glyph_index < glyph_bytes; glyph_index++)
    {
        temp = glyph[glyph_index];
        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            if ((temp & 0x80U) != 0U)
            {
                LCD_DrawFRONT_COLOR(x, y, FRONT_COLOR);
            }
            else if (mode == 0U)
            {
                LCD_DrawFRONT_COLOR(x, y, BACK_COLOR);
            }

            temp <<= 1U;
            y++;
            if (y >= tftlcd_data.height)
            {
                return;
            }

            if ((u16)(y - origin_y) == size)
            {
                y = origin_y;
                x++;
                if (x >= tftlcd_data.width)
                {
                    return;
                }
                break;
            }
        }
    }
}

/**
 * @brief 计算整数次幂
 * @param m 底数
 * @param n 指数
 * @retval `m` 的 `n` 次幂
 * @details
 * 当前主要服务于十进制数字拆位显示，用于按位提取某一位数字。
 */
u32 LCD_Pow(u8 m,u8 n)
{
	u32 result=1;
	while(n--)result*=m;
	return result;
}

/**
 * @brief 显示十进制无符号整数，高位 0 不显示
 * @param x 数字左上角起始 X 坐标
 * @param y 数字左上角起始 Y 坐标
 * @param num 要显示的数值
 * @param len 总显示位数
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @details
 * 该接口会把前导 0 位置用空格代替，因此更适合普通数值显示。
 */
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)
{
	u8 t,temp;
	u8 enshow=0;
    u8 char_width;

    char_width = LCD_GetAsciiCharWidth(size);
    if ((char_width == 0U) || (len == 0U))
    {
        return;
    }
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar((u16)(x + char_width * t), y, ' ', size, 0U);
				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar((u16)(x + char_width * t), y, (u8)(temp + '0'), size, 0U);
	}
}

/**
 * @brief 显示十进制无符号整数，可配置前导位填充方式
 * @param x 数字左上角起始 X 坐标
 * @param y 数字左上角起始 Y 坐标
 * @param num 要显示的数值
 * @param len 总显示位数
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param mode 显示模式
 * @details
 * - `mode bit7 = 0`：前导 0 用空格填充
 * - `mode bit7 = 1`：前导 0 直接显示为字符 `0`
 * - `mode bit0 = 0`：非叠加显示
 * - `mode bit0 = 1`：叠加显示
 */
void LCD_ShowxNum(u16 x,u16 y,u32 num,u8 len,u8 size,u8 mode)
{
	u8 t,temp;
	u8 enshow=0;
    u8 char_width;

    char_width = LCD_GetAsciiCharWidth(size);
    if ((char_width == 0U) || (len == 0U))
    {
        return;
    }
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				if((mode&0X80U) != 0U)LCD_ShowChar((u16)(x + char_width * t), y, '0', size, (u8)(mode&0X01U));
				else LCD_ShowChar((u16)(x + char_width * t), y, ' ', size, (u8)(mode&0X01U));
  				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar((u16)(x + char_width * t), y, (u8)(temp+'0'), size, (u8)(mode&0X01U));
	}
}

/**
 * @brief 显示 ASCII 字符串
 * @param x 字符串显示区域左上角 X 坐标
 * @param y 字符串显示区域左上角 Y 坐标
 * @param width 显示区域宽度
 * @param height 显示区域高度
 * @param size 字体高度，可选 `12`、`16`、`24`
 * @param p 字符串首地址
 * @details
 * 该接口适合显示纯英文、数字和符号。
 * 如果文本里包含中文，应优先使用 `LCD_ShowTextMixed()` 或 `LCD_ShowChinese()`。
 */
void LCD_ShowString(u16 x,u16 y,u16 width,u16 height,u8 size,u8 *p)
{
	u16 x0=x;
    u16 x_limit;
    u16 y_limit;
    u8 char_width;

    if (p == NULL)
    {
        return;
    }

    char_width = LCD_GetAsciiCharWidth(size);
    if ((char_width == 0U) || (width == 0U) || (height == 0U))
    {
        return;
    }

	x_limit = (u16)(x + width);
	y_limit = (u16)(y + height);
    while((*p<='~')&&(*p>=' '))
    {
        if ((u16)(x + char_width) > x_limit)
        {
            x = x0;
            y = (u16)(y + size);
        }
        if ((u16)(y + size) > y_limit)
        {
            break;
        }
        LCD_ShowChar(x,y,*p,size,0U);
        x = (u16)(x + char_width);
        p++;
    }
}

/* 绘制 16x16 字模 */
static void LCD_DrawGlyph16x16(uint16_t x, uint16_t y, const uint8_t *glyph)
{
    uint16_t row;
    uint16_t col;

    for (row = 0; row < 16; row++)
    {
        uint16_t bits = ((uint16_t)glyph[row * 2] << 8) | glyph[row * 2 + 1];
        for (col = 0; col < 16; col++)
        {
            if (bits & (0x8000U >> col))
            {
                LCD_DrawFRONT_COLOR((u16)(x + col), (u16)(y + row), FRONT_COLOR);
            }
            else
            {
                LCD_DrawFRONT_COLOR((u16)(x + col), (u16)(y + row), BACK_COLOR);
            }
        }
    }
}

static void LCD_DrawPlaceholder16x16(uint16_t x, uint16_t y)
{
    LCD_DrawLine_Color(x, y, x + 15, y, RED);
    LCD_DrawLine_Color(x, y + 15, x + 15, y + 15, RED);
    LCD_DrawLine_Color(x, y, x, y + 15, RED);
    LCD_DrawLine_Color(x + 15, y, x + 15, y + 15, RED);
    LCD_DrawLine_Color(x + 3, y + 3, x + 12, y + 12, RED);
    LCD_DrawLine_Color(x + 12, y + 3, x + 3, y + 12, RED);
}

static uint8_t LCD_Utf8DecodeCodepoint(const uint8_t *in, uint16_t in_len, uint16_t *consumed, uint32_t *codepoint)
{
    uint8_t c0;

    if (in == NULL || consumed == NULL || codepoint == NULL || in_len == 0U)
    {
        return 0U;
    }

    c0 = in[0];
    if (c0 < 0x80U)
    {
        *consumed = 1U;
        *codepoint = c0;
        return 1U;
    }

    if ((c0 & 0xE0U) == 0xC0U && in_len >= 2U &&
        (in[1] & 0xC0U) == 0x80U)
    {
        *consumed = 2U;
        *codepoint = ((uint32_t)(c0 & 0x1FU) << 6) |
                     (uint32_t)(in[1] & 0x3FU);
        return 1U;
    }

    if ((c0 & 0xF0U) == 0xE0U && in_len >= 3U &&
        (in[1] & 0xC0U) == 0x80U &&
        (in[2] & 0xC0U) == 0x80U)
    {
        *consumed = 3U;
        *codepoint = ((uint32_t)(c0 & 0x0FU) << 12) |
                     ((uint32_t)(in[1] & 0x3FU) << 6) |
                     (uint32_t)(in[2] & 0x3FU);
        return 1U;
    }

    if ((c0 & 0xF8U) == 0xF0U && in_len >= 4U &&
        (in[1] & 0xC0U) == 0x80U &&
        (in[2] & 0xC0U) == 0x80U &&
        (in[3] & 0xC0U) == 0x80U)
    {
        *consumed = 4U;
        *codepoint = ((uint32_t)(c0 & 0x07U) << 18) |
                     ((uint32_t)(in[1] & 0x3FU) << 12) |
                     ((uint32_t)(in[2] & 0x3FU) << 6) |
                     (uint32_t)(in[3] & 0x3FU);
        return 1U;
    }

    return 0U;
}

static uint8_t LCD_TextUtf8ToGb2312(const char *utf8, uint8_t *out, uint16_t out_cap)
{
    const uint8_t *in = (const uint8_t *)utf8;
    uint16_t i = 0U;
    uint16_t in_len;
    uint16_t o = 0U;

    if (utf8 == NULL || out == NULL || out_cap == 0U)
    {
        return 0U;
    }

    in_len = (uint16_t)strlen(utf8);
    while (i < in_len && o + 1U < out_cap)
    {
        if (in[i] < 0x80U)
        {
            out[o++] = in[i++];
            continue;
        }

        {
            uint16_t consumed = 0U;
            uint32_t codepoint = 0U;

            if (LCD_Utf8DecodeCodepoint(&in[i], (uint16_t)(in_len - i), &consumed, &codepoint) != 0U)
            {
                if (codepoint <= 0xFFFFU)
                {
                    WCHAR oem = ff_convert((WCHAR)codepoint, 0U);
                    if (oem != 0U)
                    {
                        if (oem < 0x100U)
                        {
                            out[o++] = (uint8_t)oem;
                        }
                        else
                        {
                            if (o + 2U >= out_cap)
                            {
                                break;
                            }
                            out[o++] = (uint8_t)((oem >> 8) & 0xFFU);
                            out[o++] = (uint8_t)(oem & 0xFFU);
                        }
                    }
                    else
                    {
                        out[o++] = '?';
                    }
                }
                else
                {
                    out[o++] = '?';
                }

                i = (uint16_t)(i + consumed);
                continue;
            }
        }

        out[o++] = '?';
        i++;
    }

    out[o] = '\0';
    return (o > 0U) ? 1U : 0U;
}

/**
 * @brief 显示 ASCII 与中文混排文本
 * @param text 输入字节流
 * @details
 * 这是当前工程最关键的文本显示接口之一：
 * - ASCII 部分直接按字符显示；
 * - 中文部分通过 `font_codec + font_storage` 解析并取字模；
 * - 最终统一绘制到 LCD。
 *
 * 上层 UI 的 UTF-8 文本显示，通常会先转成 GB2312，再走这个接口。
 */
void LCD_ShowTextMixed(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *text)
{
    uint16_t x0 = x;
    uint16_t x_end = x + width;
    uint16_t y_end = y + height;
    uint8_t glyph[32];

    if (text == NULL)
    {
        return;
    }

    while (*text != '\0' && y + 16 <= y_end)
    {
        FontCodecToken token;
        FontCodecResult parse_ret = FontCodec_ParseGB2312(text, &token);

        if (*text == '\r')
        {
            text++;
            continue;
        }
        if (*text == '\n')
        {
            x = x0;
            y = (uint16_t)(y + 16);
            text++;
            continue;
        }

        if (parse_ret == FONT_CODEC_ASCII)
        {
            if (x + 8 > x_end)
            {
                x = x0;
                y = (uint16_t)(y + 16);
                continue;
            }
            if (token.ascii_char >= ' ' && token.ascii_char <= '~')
            {
                LCD_ShowChar(x, y, token.ascii_char, 16, 0);
            }
            else
            {
                LCD_ShowChar(x, y, ' ', 16, 0);
            }
            x = (uint16_t)(x + 8);
            text += token.consumed;
            continue;
        }

        if (parse_ret == FONT_CODEC_OK)
        {
            FontStorageResult storage_ret;

            if (x + 16 > x_end)
            {
                x = x0;
                y = (uint16_t)(y + 16);
                continue;
            }

            storage_ret = FontStorage_ReadGlyph16(token.glyph_offset, glyph);
            if (storage_ret == FONT_STORAGE_OK)
            {
                LCD_DrawGlyph16x16(x, y, glyph);
            }
            else
            {
                LCD_DrawPlaceholder16x16(x, y);
            }

            x = (uint16_t)(x + 16);
            text += token.consumed;
            continue;
        }

        text++;
    }
}

/****************************************************************************
*鍑芥暟鍚嶏細LCD_ShowFontHZ
*杈? 鍏ワ細x锛氭眽瀛楁樉绀虹殑X鍧愭爣
*      * y锛氭眽瀛楁樉绀虹殑Y鍧愭爣
*      * cn锛氳鏄剧ず鐨勬眽瀛?*      * wordColor锛氭枃瀛楃殑棰滆壊
*      * backColor锛氳儗鏅鑹?*杈? 鍑猴細
*鍔? 鑳斤細鍐欎簩鍙锋シ浣撴眽瀛?****************************************************************************/
#if 0
void LCD_ShowFontHZ(u16 x, u16 y, u8 *cn)
{
	u8 i, j, wordNum;
	u16 color;
	while (*cn != '\0')
	{
		LCD_Set_Window(x, y, x+31, y+28);
		for (wordNum=0; wordNum<20; wordNum++)
		{	//wordNum鎵弿瀛楀簱鐨勫瓧鏁?			if ((CnChar32x29[wordNum].Index[0]==*cn)
			     &&(CnChar32x29[wordNum].Index[1]==*(cn+1)))
			{
				for(i=0; i<116; i++)
				{	//MSK鐨勪綅鏁?					color=CnChar32x29[wordNum].Msk[i];
					for(j=0;j<8;j++)
					{
						if((color&0x80)==0x80)
						{
							LCD_WriteData_Color(FRONT_COLOR);
						}
						else
						{
							LCD_WriteData_Color(BACK_COLOR);
						}
						color<<=1;
					}//for(j=0;j<8;j++)缁撴潫
				}
			}
		} //for (wordNum=0; wordNum<20; wordNum++)缁撴潫
		cn += 2;
		x += 32;
	}
}
#endif


#if 1

// 鍋囪锛?// - Chinese_Count 鏄眽瀛楁€绘暟锛堜緥濡?24锛?// - Chinese_Index_UTF8 鏄綘涔嬪墠杞崲濂界殑 UTF-8 瀛楄妭鏁扮粍锛堟瘡涓」鏈€澶?3 瀛楄妭鏈夋晥锛?// - Chinese_Font16x16 鏄搴旂殑 16x16 鐐归樀瀛椾綋鏁版嵁

/**
 * @brief 显示中文字符串
 * @details
 * 这个接口更适合“纯中文或以中文为主”的场景。
 * 如果字符串中混有英文、数字、换行等复杂布局需求，优先使用混排接口。
 */
void LCD_ShowChinese(uint16_t x, uint16_t y, const char *str)
{
    uint8_t glyph[32];

    if (str == NULL)
    {
        return;
    }

    while (*str != '\0')
    {
        FontCodecToken token;
        FontCodecResult parse_ret = FontCodec_ParseGB2312((const uint8_t *)str, &token);

        if (parse_ret == FONT_CODEC_ASCII)
        {
            LCD_ShowChar(x, y, token.ascii_char, 16, 0);
            x = (uint16_t)(x + 8U);
            str += token.consumed;
            continue;
        }

        if (parse_ret == FONT_CODEC_OK)
        {
            if (FontStorage_ReadGlyph16(token.glyph_offset, glyph) == FONT_STORAGE_OK)
            {
                LCD_DrawGlyph16x16(x, y, glyph);
            }
            else
            {
                LCD_DrawPlaceholder16x16(x, y);
            }

            x = (uint16_t)(x + 16U);
            str += token.consumed;
            continue;
        }

        x = (uint16_t)(x + 8U);
        str++;
    }
}

void LCD_ShowTextUtf8(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *utf8)
{
    uint8_t gb[256];

    if (utf8 == NULL)
    {
        return;
    }

    if (LCD_TextUtf8ToGb2312(utf8, gb, (uint16_t)sizeof(gb)) != 0U)
    {
        LCD_ShowTextMixed(x, y, width, height, gb);
    }
    else
    {
        LCD_ShowString(x, y, width, height, 16U, (uint8_t *)utf8);
    }
}
// void LCD_ShowChinese(uint16_t x, uint16_t y, const char *str)
// {
//     uint16_t x0 = x;
//     uint16_t i, row, col;
//     const uint8_t *font;
//     uint16_t data;
//
//     while (*str)
//     {
//         /* 璺宠繃 ASCII锛堢┖鏍笺€佹爣鐐圭瓑锛?*/
//         if ((uint8_t)*str < 0x80)
//         {
//             x += 8;      // ASCII 鍗犱綅
//             str++;
//             continue;
//         }
//
//         font = NULL;
//
//         /* 鍦ㄦ眽瀛楃储寮曡〃涓煡鎵?*/
//         for (i = 0; i < Chinese_Count; i++)
//         {
//             if ((uint8_t)str[0] == (uint8_t)Chinese_Index[i][0] &&
//                 (uint8_t)str[1] == (uint8_t)Chinese_Index[i][1])
//             {
//                 font = Chinese_Font16x16[i];
//                 break;
//             }
//         }
//
//         /* 鎵句笉鍒板氨璺宠繃 */
//         if (font == NULL)
//         {
//             str += 2;
//             continue;
//         }
//
//         /* 缁樺埗 16x16 姹夊瓧锛堜綆浣嶅湪宸︼級 */
//         for (row = 0; row < 16; row++)
//         {
//             data = font[row * 2] |
//                   (font[row * 2 + 1] << 8);
//
//             for (col = 0; col < 16; col++)
//             {
//                 if (data & (1 << col))
//                 {
//                     LCD_DrawFRONT_COLOR(x + col,
//                                         y + row,
//                                         FRONT_COLOR);
//                 }
//             }
//         }
//
//         /* 涓嬩竴涓眽瀛?*/
//         x += 16;
//         str += 2;
//     }
// }




void LCD_ShowFontHZ(u16 x, u16 y, u8 *cn)
{
	u8 i, j, wordNum;
	u16 color;
	u16 x0=x;
	u16 y0=y;
	while (*cn != '\0')
	{
		for (wordNum=0; wordNum<20; wordNum++)
		{	//wordNum鎵弿瀛楀簱鐨勫瓧鏁?
            if ((CnChar32x29[wordNum].Index[0]==*cn)
			     &&(CnChar32x29[wordNum].Index[1]==*(cn+1)))
			{
				for(i=0; i<116; i++)
				{	//MSK鐨勪綅鏁?					color=CnChar32x29[wordNum].Msk[i];
					for(j=0;j<8;j++)
					{
						if((color&0x80)==0x80)
						{
							LCD_DrawFRONT_COLOR(x,y,FRONT_COLOR);
						}
						else
						{
							LCD_DrawFRONT_COLOR(x,y,BACK_COLOR);
						}
						color<<=1;
						x++;
						if((x-x0)==32)
						{
							x=x0;
							y++;
							if((y-y0)==29)
							{
								y=y0;
							}
						}
					}//for(j=0;j<8;j++)缁撴潫
				}
			}

		} //for (wordNum=0; wordNum<20; wordNum++)缁撴潫
		cn += 2;
		x += 32;
		x0=x;
	}
}
#endif

void LCD_ShowPicture(u16 x, u16 y, u16 wide, u16 high,u8 *pic)
{
	u16 temp = 0;
	long tmp=0,num=0;
	LCD_Set_Window(x, y, x+wide-1, y+high-1);
	num = wide * high*2 ;
	do
	{
		temp = pic[tmp + 1];
		temp = temp << 8;
		temp = temp | pic[tmp];
		LCD_WriteData_Color(temp);//閫愮偣鏄剧ず
		tmp += 2;
	}
	while(tmp < num);
}


