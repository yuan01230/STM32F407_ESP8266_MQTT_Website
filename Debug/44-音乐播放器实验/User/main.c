#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "tftlcd.h" 
#include "malloc.h" 
#include "sdio_sdcard.h" 
#include "flash.h"
#include "ff.h" 
#include "fatfs_app.h"
#include "key.h"
#include "font_show.h"
#include "wm8978.h"	
#include "audioplay.h"



int main()
{	
	
	SysTick_Init(168);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	LED_Init();
	KEY_Init();
	USART1_Init(115200);
	TFTLCD_Init();			//LCD初始化
	EN25QXX_Init();				//初始化EN25Q128	  
	WM8978_Init();				//初始化WM8978
	WM8978_HPvol_Set(40,40);	//耳机音量设置
	WM8978_SPKvol_Set(50);		//喇叭音量设置
	
	my_mem_init(SRAMIN);		//初始化内部内存池
	my_mem_init(SRAMCCM);		//初始化CCM内存池
	
	FRONT_COLOR=RED;//设置字体为红色 
	while(SD_Init()!=0)
	{	
		LCD_ShowString(10,10,tftlcd_data.width,tftlcd_data.height,16,"SD Card Error!");
	}
	FATFS_Init();							//为fatfs相关变量申请内存				 
  	f_mount(fs[0],"0:",1); 					//挂载SD卡 
 	f_mount(fs[1],"1:",1); 				//挂载FLASH.
	while(font_init()) 		        //检查字库
	{  
		LCD_ShowString(10,10,tftlcd_data.width,tftlcd_data.height,16,"Font Error!   ");
		delay_ms(500);
	}
	LCD_ShowFontString(60,50,200,16,"PRECHIN-STM32F4开发板",16,0);				    	 
	LCD_ShowFontString(60,70,200,16,"音乐播放器实验",16,0);				    	 
	LCD_ShowFontString(60,90,200,16,"www.prechin.net",16,0);				    	 
	LCD_ShowFontString(60,110,200,16,"KEY0:NEXT   KEY2:PREV",16,0); 
	LCD_ShowFontString(60,130,200,16,"KEY_UP:PAUSE/PLAY",16,0);
	while(1)
	{
		audio_play();	
	}
}
