//
// Created by 鏍囪 on 25-12-30.
//

/**
 ******************************************************************************
 * @file    tftlcd.c
 * @brief   HX8357DN TFT LCD driver (HAL + FSMC)
 ******************************************************************************
 */

#include "tftlcd.h"

#include "tftlcd.h"
#include "stdlib.h"
#include "font.h"
#include "string.h"
#include "../font_storage/font_storage.h"
#include "../font_codec/font_codec.h"

//LCD鐨勭敾绗旈鑹插拰鑳屾櫙鑹?
u16 FRONT_COLOR=BLACK;	//鐢荤瑪棰滆壊
u16 BACK_COLOR=WHITE;  //鑳屾櫙鑹?
_tftlcd_data tftlcd_data;


//鍐欏瘎瀛樺櫒鍑芥暟
//cmd:瀵勫瓨鍣ㄥ€?void LCD_WriteCmd(u16 cmd)
void LCD_WriteCmd(u16 cmd)
{


#ifdef TFTLCD_HX8357DN
	TFTLCD->LCD_CMD=cmd;
#endif


}

//鍐欐暟鎹?//data:瑕佸啓鍏ョ殑鍊?void LCD_WriteData(u16 data)
void LCD_WriteData(u16 data)
{

#ifdef TFTLCD_HX8357DN
	TFTLCD->LCD_DATA=data;
#endif


}

void LCD_WriteCmdData(u16 cmd,u16 data)
{
	LCD_WriteCmd(cmd);
	LCD_WriteData(data);
}


u32 LCD_RGBColor_Change(u16 color)
{
	u8 r,g,b=0;

	r=(color>>11)&0x1f;
	g=(color>>5)&0x3f;
	b=color&0x1f;

	return ((r<<13)|(g<<6)|(b<<1));
}
void LCD_WriteData_Color(u16 color)
{

#ifdef TFTLCD_HX8357DN
	TFTLCD->LCD_DATA=color>>8;
	TFTLCD->LCD_DATA=color&0xff;
#endif

}

//璇绘暟鎹?//杩斿洖鍊?璇诲埌鐨勫€?u16 LCD_ReadData(void)
u16 LCD_ReadData(void)
{


#ifdef TFTLCD_HX8357DN
//	u16 ram1,ram2;
//	ram1=TFTLCD->LCD_DATA;
//	printf("ram1=%x   ",ram1);
//	ram2=TFTLCD->LCD_DATA;
//	printf("ram2=%x   \r\n",ram2);
//	ram2=ram2<<8|ram1;
//	return ram2;
	return TFTLCD->LCD_DATA;
//	return ((TFTLCD->LCD_DATA<<8)|(TFTLCD->LCD_DATA));
#endif

}


//璁剧疆LCD鏄剧ず鏂瑰悜
//dir:0,绔栧睆锛?,妯睆
void LCD_Display_Dir(u8 dir)
{
	tftlcd_data.dir=dir;         //妯睆/绔栧睆
	if(dir==0)  //榛樿绔栧睆鏂瑰悜
	{


#ifdef TFTLCD_HX8357DN
		LCD_WriteCmd(0x36);   //璁剧疆褰╁睆鏄剧ず鏂瑰悜鐨勫瘎瀛樺櫒
		LCD_WriteData(0x4c);
		tftlcd_data.height=480;
		tftlcd_data.width=320;
#endif

	}
	else
	{

#ifdef TFTLCD_HX8357DN
		LCD_WriteCmd(0x36);
		LCD_WriteData(0x2c);
		tftlcd_data.height=320;
		tftlcd_data.width=480;
#endif


	}
}


//璁剧疆绐楀彛,骞惰嚜鍔ㄨ缃敾鐐瑰潗鏍囧埌绐楀彛宸︿笂瑙?sx,sy).
//sx,sy:绐楀彛璧峰鍧愭爣(宸︿笂瑙?
//width,height:绐楀彛瀹藉害鍜岄珮搴?蹇呴』澶т簬0!!
//绐椾綋澶у皬:width*height.
void LCD_Set_Window(u16 sx,u16 sy,u16 width,u16 height)
{

#ifdef TFTLCD_HX8357DN
	LCD_WriteCmd(0x2A);
    LCD_WriteData(sx>>8);
    LCD_WriteData(sx&0XFF);
    LCD_WriteData(width>>8);
    LCD_WriteData(width&0XFF);

    LCD_WriteCmd(0x2b);
    LCD_WriteData(sy>>8);
    LCD_WriteData(sy&0XFF);
    LCD_WriteData(height>>8);
    LCD_WriteData(height&0XFF);
    LCD_WriteCmd(0x2c);
#endif

}

//璇诲彇涓煇鐐圭殑棰滆壊鍊?//x,y:鍧愭爣
//杩斿洖鍊?姝ょ偣鐨勯鑹?u16 LCD_ReadPoint(u16 x,u16 y)
u16 LCD_ReadPoint(u16 x,u16 y)
{
 	u16 r=0,g=0,b=0;
	u16 r1,r2,r3;
	u32 value;

	if(x>=tftlcd_data.width||y>=tftlcd_data.height)return 0;	//瓒呰繃浜嗚寖鍥?鐩存帴杩斿洖
	LCD_Set_Window(x, y, x, y);

#ifdef TFTLCD_HX8357D
	LCD_WriteCmd(0X2E);
 	r=LCD_ReadData();								//dummy Read
 	r=LCD_ReadData();  		  						//瀹為檯鍧愭爣棰滆壊
#endif

#ifdef TFTLCD_HX8357DN
//	LCD_WriteCmd(0X2E);
//	r=TFTLCD->LCD_DATA;
//	r=TFTLCD->LCD_DATA<<8;
//	r|=TFTLCD->LCD_DATA;
	LCD_WriteCmd(0X2E);
	r=TFTLCD->LCD_DATA;
	r=TFTLCD->LCD_DATA;
	g=TFTLCD->LCD_DATA;
	b=TFTLCD->LCD_DATA;
	r=r<<8|(g&0xff);
#endif

 	return r;
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
	u16 i;

	// TFTLCD_GPIO_Init();
	// TFTLCD_FSMC_Init();

	HAL_Delay(50);



#ifdef TFTLCD_HX8357DN
	LCD_WriteCmd(0Xd0);
	tftlcd_data.id=LCD_ReadData();	//dummy read
	tftlcd_data.id=LCD_ReadData();
#endif








 	// printf(" LCD ID:%x\r\n",tftlcd_data.id); //鎵撳嵃LCD ID



#ifdef TFTLCD_HX8357DN
	LCD_WriteCmd(0xE9);
	LCD_WriteData(0x20);

	LCD_WriteCmd(0x11); //Exit Sleep
	for(i=500; i>0; i--);

	LCD_WriteCmd(0x3A);
	LCD_WriteData(0x55);  //16Bit colors

	LCD_WriteCmd(0xD1);
	LCD_WriteData(0x00);
	LCD_WriteData(0x65); //璋冭瘯姝ゅ€兼敼鍠勬按绾?	LCD_WriteData(0x1F);

	LCD_WriteCmd(0xD0);
	LCD_WriteData(0x07);
	LCD_WriteData(0x07);
	LCD_WriteData(0x80);

	LCD_WriteCmd(0x36); 	 //Set_address_mode
	LCD_WriteData(0x4c);   	//4c

	LCD_WriteCmd(0xC1);
	LCD_WriteData(0x10);
	LCD_WriteData(0x10);
	LCD_WriteData(0x02);
	LCD_WriteData(0x02);

	LCD_WriteCmd(0xC0); //Set Default Gamma
	LCD_WriteData(0x00);
	LCD_WriteData(0x35);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x01);
	LCD_WriteData(0x02);

	LCD_WriteCmd(0xC4);
	LCD_WriteData(0x03);

	LCD_WriteCmd(0xC5); //Set frame rate
	LCD_WriteData(0x01);

	LCD_WriteCmd(0xD2); //power setting
	LCD_WriteData(0x01);
	LCD_WriteData(0x22);

	LCD_WriteCmd(0xE7);
	LCD_WriteData(0x38);

	LCD_WriteCmd(0xF3);
    LCD_WriteData(0x08);
	LCD_WriteData(0x12);
	LCD_WriteData(0x12);
	LCD_WriteData(0x08);

	LCD_WriteCmd(0xC8); //Set Gamma
	LCD_WriteData(0x01);
	LCD_WriteData(0x52);
	LCD_WriteData(0x37);
	LCD_WriteData(0x10);
	LCD_WriteData(0x0d);
	LCD_WriteData(0x01);
	LCD_WriteData(0x04);
	LCD_WriteData(0x51);
	LCD_WriteData(0x77);
	LCD_WriteData(0x01);
	LCD_WriteData(0x01);
	LCD_WriteData(0x0d);
	LCD_WriteData(0x08);
	LCD_WriteData(0x80);
	LCD_WriteData(0x00);

    LCD_WriteCmd(0x29);
#endif


	LCD_Display_Dir(TFTLCD_DIR);		//0锛氱珫灞? 1锛氭í灞? 榛樿绔栧睆
	LCD_Clear(WHITE);
}

//娓呭睆鍑芥暟
//color:瑕佹竻灞忕殑濉厖鑹?void LCD_Clear(u16 color)
void LCD_Clear(u16 color)
{
	uint16_t i, j ;

	LCD_Set_Window(0, 0, tftlcd_data.width-1, tftlcd_data.height-1);	 //浣滅敤鍖哄煙
  	for(i=0; i<tftlcd_data.width; i++)
	{
		for (j=0; j<tftlcd_data.height; j++)
		{
			LCD_WriteData_Color(color);
		}
	}
}

// 娓呴櫎鎸囧畾鍖哄煙锛堝～鍏呴鑹诧級
void LCD_ClearArea(uint16_t x0, uint16_t y0,
                   uint16_t x1, uint16_t y1)
{
    uint16_t i, j;
    uint16_t width, height;

    /* 杈圭晫淇濇姢锛堥槻姝㈣秺鐣岋級 */
    if (x1 >= tftlcd_data.width)  x1 = tftlcd_data.width - 1;
    if (y1 >= tftlcd_data.height) y1 = tftlcd_data.height - 1;
    if (x0 > x1 || y0 > y1) return;

    width  = x1 - x0 + 1;
    height = y1 - y0 + 1;

    /* 璁剧疆鏄剧ず绐楀彛 */
    LCD_Set_Window(x0, y0, x1, y1);

    /* 濉厖棰滆壊 */
    for (i = 0; i < width; i++)
    {
        for (j = 0; j < height; j++)
        {
            LCD_WriteData_Color(WHITE);
        }
    }
}

//鍦ㄦ寚瀹氬尯鍩熷唴濉厖鍗曚釜棰滆壊
//(sx,sy),(ex,ey):濉厖鐭╁舰瀵硅鍧愭爣,鍖哄煙澶у皬涓?(ex-sx+1)*(ey-sy+1)
//color:瑕佸～鍏呯殑棰滆壊
void LCD_Fill(u16 xState,u16 yState,u16 xEnd,u16 yEnd,u16 color)
{
	uint16_t temp;

    if((xState > xEnd) || (yState > yEnd))
    {
        return;
    }
	LCD_Set_Window(xState, yState, xEnd, yEnd);
    xState = xEnd - xState + 1;
	yState = yEnd - yState + 1;

	while(xState--)
	{
	 	temp = yState;
		while (temp--)
	 	{
			LCD_WriteData_Color(color);
		}
	}
}

//鍦ㄦ寚瀹氬尯鍩熷唴濉厖鎸囧畾棰滆壊鍧?//(sx,sy),(ex,ey):濉厖鐭╁舰瀵硅鍧愭爣,鍖哄煙澶у皬涓?(ex-sx+1)*(ey-sy+1)
//color:瑕佸～鍏呯殑棰滆壊
void LCD_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color)
{
	u16 height,width;
	u16 i,j;
	width=ex-sx+1; 			//寰楀埌濉厖鐨勫搴?	height=ey-sy+1;			//楂樺害

	for(i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			LCD_Set_Window(sx+j, sy+i,ex, ey);
			LCD_WriteData_Color(color[i*width+j]);
		}
	}
}
//鐢荤偣
//x,y:鍧愭爣
//FRONT_COLOR:姝ょ偣鐨勯鑹?void LCD_DrawPoint(u16 x,u16 y)
void LCD_DrawPoint(u16 x,u16 y)
{
	LCD_Set_Window(x, y, x, y);  //璁剧疆鐐圭殑浣嶇疆
	LCD_WriteData_Color(FRONT_COLOR);
}

//蹇€熺敾鐐?//x,y:鍧愭爣
//color:棰滆壊
void LCD_DrawFRONT_COLOR(u16 x,u16 y,u16 color)
{
	LCD_Set_Window(x, y, x, y);
	LCD_WriteData_Color(color);
}

//鐢荤嚎
//x1,y1:璧风偣鍧愭爣
//x2,y2:缁堢偣鍧愭爣
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


// 鐢讳竴涓崄瀛楃殑鏍囪
// x锛氭爣璁扮殑X鍧愭爣
// y锛氭爣璁扮殑Y鍧愭爣
// color锛氭爣璁扮殑棰滆壊
void LCD_DrowSign(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t i;

    /* 鐢荤偣 */
    LCD_Set_Window(x-1, y-1, x+1, y+1);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);
    }

    /* 鐢荤珫 */
    LCD_Set_Window(x-4, y, x+4, y);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);
    }

    /* 鐢绘í */
    LCD_Set_Window(x, y-4, x, y+4);
    for(i=0; i<9; i++)
    {
		LCD_WriteData_Color(color);
    }
}

//鐢荤煩褰?//(x1,y1),(x2,y2):鐭╁舰鐨勫瑙掑潗鏍?void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}
//鍦ㄦ寚瀹氫綅缃敾涓€涓寚瀹氬ぇ灏忕殑鍦?//(x,y):涓績鐐?//r    :鍗婂緞
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



//鍦ㄦ寚瀹氫綅缃樉绀轰竴涓瓧绗?//x,y:璧峰鍧愭爣
//num:瑕佹樉绀虹殑瀛楃:" "--->"~"
//size:瀛椾綋澶у皬 12/16/24
//mode:鍙犲姞鏂瑰紡(1)杩樻槸闈炲彔鍔犳柟寮?0)
void LCD_ShowChar(u16 x,u16 y,u8 num,u8 size,u8 mode)
{
    u8 temp,t1,t;
	u16 y0=y;
	u8 csize=(size/8+((size%8)?1:0))*(size/2);		//寰楀埌瀛椾綋涓€涓瓧绗﹀搴旂偣闃甸泦鎵€鍗犵殑瀛楄妭鏁?
	num=num-' ';//寰楀埌鍋忕Щ鍚庣殑鍊硷紙ASCII瀛楀簱鏄粠绌烘牸寮€濮嬪彇妯★紝鎵€浠?' '灏辨槸瀵瑰簲瀛楃鐨勫瓧搴擄級
	for(t=0;t<csize;t++)
	{
		if(size==12)temp=ascii_1206[num][t]; 	 	//璋冪敤1206瀛椾綋
		else if(size==16)temp=ascii_1608[num][t];	//璋冪敤1608瀛椾綋
		else if(size==24)temp=ascii_2412[num][t];	//璋冪敤2412瀛椾綋
		else return;								//娌℃湁鐨勫瓧搴?
		for(t1=0;t1<8;t1++)
		{
			if(temp&0x80)LCD_DrawFRONT_COLOR(x,y,FRONT_COLOR);
			else if(mode==0)LCD_DrawFRONT_COLOR(x,y,BACK_COLOR);
			temp<<=1;
			y++;
			if(y>=tftlcd_data.height)return;		//瓒呭尯鍩熶簡
			if((y-y0)==size)
			{
				y=y0;
				x++;
				if(x>=tftlcd_data.width)return;	//瓒呭尯鍩熶簡
				break;
			}
		}
	}
}
//m^n鍑芥暟
//杩斿洖鍊?m^n娆℃柟.
u32 LCD_Pow(u8 m,u8 n)
{
	u32 result=1;
	while(n--)result*=m;
	return result;
}
//鏄剧ず鏁板瓧,楂樹綅涓?,鍒欎笉鏄剧ず
//x,y :璧风偣鍧愭爣
//len :鏁板瓧鐨勪綅鏁?//size:瀛椾綋澶у皬
//color:棰滆壊
//num:鏁板€?0~4294967295);
void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)
{
	u8 t,temp;
	u8 enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+(size/2)*t,y,' ',size,0);
				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar(x+(size/2)*t,y,temp+'0',size,0);
	}
}

//鏄剧ず鏁板瓧,楂樹綅涓?,杩樻槸鏄剧ず
//x,y:璧风偣鍧愭爣
//num:鏁板€?0~999999999);
//len:闀垮害(鍗宠鏄剧ず鐨勪綅鏁?
//size:瀛椾綋澶у皬
//mode:
//[7]:0,涓嶅～鍏?1,濉厖0.
//[6:1]:淇濈暀
//[0]:0,闈炲彔鍔犳樉绀?1,鍙犲姞鏄剧ず.
void LCD_ShowxNum(u16 x,u16 y,u32 num,u8 len,u8 size,u8 mode)
{
	u8 t,temp;
	u8 enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				if(mode&0X80)LCD_ShowChar(x+(size/2)*t,y,'0',size,mode&0X01);
				else LCD_ShowChar(x+(size/2)*t,y,' ',size,mode&0X01);
 				continue;
			}else enshow=1;

		}
	 	LCD_ShowChar(x+(size/2)*t,y,temp+'0',size,mode&0X01);
	}
}
//鏄剧ず瀛楃涓?//x,y:璧风偣鍧愭爣
//width,height:鍖哄煙澶у皬
//size:瀛椾綋澶у皬
//*p:瀛楃涓茶捣濮嬪湴鍧€
void LCD_ShowString(u16 x,u16 y,u16 width,u16 height,u8 size,u8 *p)
{
	u8 x0=x;
	width+=x;
	height+=y;
    while((*p<='~')&&(*p>=' '))//鍒ゆ柇鏄笉鏄潪娉曞瓧绗?
    {
        if(x>=width){x=x0;y+=size;}
        if(y>=height)break;//閫€鍑?
        LCD_ShowChar(x,y,*p,size,0);
        x+=size/2;
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

void LCD_ShowChinese(uint16_t x, uint16_t y, const char *str)
{
    uint16_t i, row, col;
    const uint8_t *font;
    uint16_t data;

    while (*str)
    {
        /* ASCII: 8x16 */
        if ((uint8_t)*str < 0x80)
        {
            x += 8;
            str++;
            continue;
        }

        /* 3-byte UTF-8 */
        if (((uint8_t)str[0] & 0xF0) == 0xE0 &&
            ((uint8_t)str[1] & 0xC0) == 0x80 &&
            ((uint8_t)str[2] & 0xC0) == 0x80)
        {
            font = NULL;

            for (i = 0; i < Chinese_Count; i++)
            {
                if ((uint8_t)str[0] == Chinese_Index[i][0] &&
                    (uint8_t)str[1] == Chinese_Index[i][1] &&
                    (uint8_t)str[2] == Chinese_Index[i][2])
                {
                    font = Chinese_Font16x16[i];
                    break;
                }
            }

            if (font != NULL)
            {
                for (row = 0; row < 16; row++)
                {
                    data = font[row * 2] |
                          (font[row * 2 + 1] << 8);

                    for (col = 0; col < 16; col++)
                    {
                        if (data & (1 << col))
                        {
                            LCD_DrawFRONT_COLOR(x + col,
                                                y + row,
                                                FRONT_COLOR);
                        }
                    }
                }
                x += 16;
            }
            else
            {
                x += 16;
            }

            str += 3;
        }
        else if (((uint8_t)str[0] & 0xE0) == 0xC0 &&
                 ((uint8_t)str[1] & 0xC0) == 0x80)
        {
            x += 8;
            str += 2;
        }
        else
        {
            x += 8;
            str++;
        }
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


