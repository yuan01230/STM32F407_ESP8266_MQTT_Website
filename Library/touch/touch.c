/**
 ******************************************************************************
 * @file    touch.c
 * @author  普中科技
 * @brief   XPT2046 电阻触摸屏驱动程序
 *          
 * @功能说明:
 *          1. 使用软件模拟 SPI 与 XPT2046 通信
 *          2. 支持触摸坐标读取和滤波处理
 *          3. 支持四点校准并保存参数到 EEPROM
 *          4. 将物理 ADC 坐标转换为屏幕坐标
 *          
 * @硬件连接:
 *          T_CS (片选)     -> PC13
 *          T_SCK (时钟)    -> PB0
 *          T_MOSI (数据入) -> PF11
 *          T_MISO (数据出) -> PB2
 *          T_PEN (中断)    -> PB1
 *          
 * @版本    V2.0
 * @日期    2026-03-17
 ******************************************************************************
 */

#include "touch.h"
#include "../tftlcd/tftlcd.h"
#include "../AT24CXX/at24cxx.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * 全局变量定义
 * ============================================================================ */

/**
 * @brief 触摸屏设备实例
 * 初始化各函数指针和默认值
 */
_m_tp_dev tp_dev = 
{
    .init       = TP_Init,      /* 初始化函数 */
    .scan       = TP_Scan,      /* 扫描函数 */
    .adjust     = TP_Adjust,    /* 校准函数 */
    .x          = {0},          /* X 坐标数组清零 */
    .y          = {0},          /* Y 坐标数组清零 */
    .sta        = 0,            /* 状态寄存器清零 */
    .xfac       = 0,            /* X 比例因子清零 */
    .yfac       = 0,            /* Y 比例因子清零 */
    .xoff       = 0,            /* X 偏移量清零 */
    .yoff       = 0,            /* Y 偏移量清零 */
    .touchtype  = 0             /* 触摸类型清零 */
};

/* XPT2046 命令字定义 */
static const u8 CMD_RDX = 0xD0;  /*!< 读取 X 坐标命令 */
static const u8 CMD_RDY = 0x90;  /*!< 读取 Y 坐标命令 */

/* 外部 LCD 变量声明 */
extern _tftlcd_data tftlcd_data;

/* ============================================================================
 * 内部安全延时函数 (不依赖定时器，避免冲突)
 * ============================================================================ */

/**
 * @brief 微秒级延时函数
 * @param us 延时的微秒数
 * @note 基于 168MHz 主频粗略估算
 */
static void tp_delay_us(uint32_t us)
{
    volatile uint32_t i;
    for(i = 0; i < us * 30; i++)  /* 168MHz 下大致延时系数 */
    {
        __NOP();  /* 空操作，消耗 CPU 周期 */
    }
}

/**
 * @brief 毫秒级延时函数
 * @param ms 延时的毫秒数
 */
static void tp_delay_ms(uint32_t ms)
{
    while(ms--)
    {
        tp_delay_us(1000);  /* 调用 1000 次微秒延时 */
    }
}

/* ============================================================================
 * 底层 SPI 驱动函数
 * ============================================================================ */

/**
 * @brief 通过软件 SPI 写入一个字节数据
 * @param num: 要写入的 8 位数据
 * @note MSB First (高位先发送)，在时钟上升沿锁存数据
 */
void TP_Write_Byte(u8 num)
{
    u8 count = 0;
    
    for(count = 0; count < 8; count++)
    {
        /* 输出最高位 */
        if(num & 0x80)
        {
            TDIN(1);  /* 发送逻辑 1 */
        }
        else
        {
            TDIN(0);  /* 发送逻辑 0 */
        }
        
        num <<= 1;         /* 左移一位，准备发送下一位 */
        TCLK(0);           /* 拉低时钟 */
        tp_delay_us(1);    /* 稳定数据 */
        TCLK(1);           /* 产生上升沿，XPT2046 在上升沿锁存数据 */
    }
}

/**
 * @brief 通过软件 SPI 读取 AD 转换值
 * @param CMD: 读取命令 (CMD_RDX=0xD0 读 X, CMD_RDY=0x90 读 Y)
 * @return 12 位 ADC 转换结果 (0-4095)
 * 
 * @通信时序说明:
 *   1. 拉低 TCS 选中芯片
 *   2. 发送命令字 (8 位)
 *   3. 等待转换完成 (约 6us)
 *   4. 清除 BUSY 标志
 *   5. 读取 16 位数据 (实际有效 12 位，左对齐)
 *   6. 右移 4 位得到 12 位结果
 *   7. 拉高 TCS 取消选中
 */
u16 TP_Read_AD(u8 CMD)
{
    u8 count = 0;
    u16 Num = 0;

    /* 1. 初始化时钟和数据线，选中芯片 */
    TCLK(0);
    TDIN(0);
    TCS(0);  /* 片选有效，开始通信 */

    /* 2. 发送命令字 */
    TP_Write_Byte(CMD);

    /* 3. 等待 AD 转换完成 (XPT2046 需要约 6us 转换时间) */
    tp_delay_us(6);

    /* 4. 清除 BUSY 标志 */
    TCLK(0);
    tp_delay_us(1);
    TCLK(1);
    tp_delay_us(1);
    TCLK(0);

    /* 5. 读取 16 位数据 (XPT2046 输出 12 位数据，左对齐在 16 位中) */
    for(count = 0; count < 16; count++)
    {
        Num <<= 1;              /* 左移一位，为新数据腾出位置 */
        TCLK(0);                /* 拉低时钟 */
        tp_delay_us(1);         /* 稳定数据 */
        TCLK(1);                /* 产生上升沿 */
        
        /* XPT2046 在时钟下降沿输出数据，MCU 在高电平期间读取 */
        if(DOUT)
        {
            Num++;  /* 如果 DOUT 为高，最低位置 1 */
        }
    }

    /* 6. 数据格式转换：右移 4 位，得到 12 位有效数据 */
    Num >>= 4;  /* 12 位数据范围：0-4095 */
    
    /* 7. 取消选中芯片 */
    TCS(1);

    return Num;
}

/* ============================================================================
 * 坐标读取与滤波算法
 * ============================================================================ */

#define READ_TIMES      5   /*!< 采样次数：每次读取 5 次取平均值 */
#define LOST_VAL        1   /*!< 丢弃值：去掉最大和最小的 1 个值 */
#define ERR_RANGE       50  /*!< 误差范围：两次读取允许的最大差值 */

/**
 * @brief 读取 X 或 Y 轴的物理坐标 (带中值平均滤波)
 * @param xy: 0-读 X 轴，1-读 Y 轴
 * @return 滤波后的 ADC 值 (0-4095)
 * 
 * @滤波算法说明:
 *   1. 连续读取 5 次数据
 *   2. 使用冒泡排序对数据排序
 *   3. 去掉最大值和最小值各 1 个
 *   4. 对中间 3 个值求平均
 *   
 * @优点：能有效消除突发干扰和抖动
 */
u16 TP_Read_XOY(u8 xy)
{
    u16 i, j;
    u16 buf[READ_TIMES];   /* 存储 5 次采样值 */
    u16 sum = 0;           /* 累加和 */
    u16 temp;

    /* 1. 连续采集 5 次数据 */
    for(i = 0; i < READ_TIMES; i++)
    {
        buf[i] = TP_Read_AD(xy);  /* 读取一次 AD 值 */
    }

    /* 2. 冒泡排序 (从小到大) */
    for(i = 0; i < READ_TIMES - 1; i++)
    {
        for(j = i + 1; j < READ_TIMES; j++)
        {
            if(buf[i] > buf[j])
            {
                /* 交换两个数据 */
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }

    /* 3. 去掉最大值和最小值，计算中间值的平均 */
    /* 去掉最小的 1 个 (buf[0]) 和最大的 1 个 (buf[4]) */
    for(i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++)
    {
        sum += buf[i];  /* 累加中间的 3 个值 */
    }

    /* 4. 求平均值 */
    temp = sum / (READ_TIMES - 2 * LOST_VAL);  /* 除以 3 */
    
    return temp;
}

/**
 * @brief 读取一次触摸点的 X 和 Y 坐标
 * @param x: X 坐标指针 (输出参数)
 * @param y: Y 坐标指针 (输出参数)
 * @return 1-读取成功，0-读取失败
 * 
 * @注意：先读 X 后读 Y，因为 Y 轴读数会建立新的采样电容
 */
u8 TP_Read_XY(u16 *x, u16 *y)
{
    u16 xtemp, ytemp;
    
    /* 依次读取 X 和 Y 坐标 */
    xtemp = TP_Read_XOY(CMD_RDX);  /* 读取 X 坐标 */
    ytemp = TP_Read_XOY(CMD_RDY);  /* 读取 Y 坐标 */

    *x = xtemp;
    *y = ytemp;
    
    return 1;  /* 总是返回成功 */
}

/**
 * @brief 获取原始 ADC 坐标 (供外部调试使用)
 * @param x: X 坐标指针
 * @param y: Y 坐标指针
 */
void Touch_Get_Raw_XY(uint16_t *x, uint16_t *y)
{
    TP_Read_XY(x, y);
}

/**
 * @brief 连续读取两次坐标并校验，提高测量精度
 * @param x: X 坐标指针 (输出参数)
 * @param y: Y 坐标指针 (输出参数)
 * @return 1-校验成功，0-校验失败 (误差过大)
 * 
 * @校验原理:
 *   1. 连续读取两次坐标
 *   2. 比较两次读数的差值
 *   3. 如果差值在允许范围内 (ERR_RANGE=50)，则认为有效
 *   4. 返回两次读数的平均值
 *   
 * @优点：能过滤掉因机械振动或电磁干扰造成的异常值
 */
u8 TP_Read_XY2(u16 *x, u16 *y)
{
    u16 x1, y1;  /* 第一次读数 */
    u16 x2, y2;  /* 第二次读数 */

    /* 1. 第一次读取 */
    if(TP_Read_XY(&x1, &y1) == 0)
    {
        return 0;  /* 读取失败 */
    }
    
    /* 2. 第二次读取 */
    if(TP_Read_XY(&x2, &y2) == 0)
    {
        return 0;  /* 读取失败 */
    }

    /* 3. 校验两次读数的差值是否在允许范围内 */
    /* X 轴差值检查：|x1 - x2| < ERR_RANGE */
    /* Y 轴差值检查：|y1 - y2| < ERR_RANGE */
    if(
        ((x2 <= x1 && x1 < x2 + ERR_RANGE) || (x1 <= x2 && x2 < x1 + ERR_RANGE)) &&
        ((y2 <= y1 && y1 < y2 + ERR_RANGE) || (y1 <= y2 && y2 < y1 + ERR_RANGE))
       )
    {
        /* 4. 计算平均值作为最终结果 */
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;  /* 校验成功 */
    }

    return 0;  /* 误差过大，认为无效 */
}

/* ============================================================================
 * 校准参数存取 (使用 AT24CXX EEPROM)
 * ============================================================================ */

/**
 * @brief 保存校准参数到 EEPROM
 * 
 * @存储格式 (从地址 40 开始):
 *   Addr 40:     校准标志 (0x0A)
 *   Addr 41-44:  xfac (4 字节 float，放大 100000000 倍后存为 int32)
 *   Addr 45-48:  yfac (4 字节 float，放大 100000000 倍后存为 int32)
 *   Addr 49-50:  xoff (2 字节 short)
 *   Addr 51-52:  yoff (2 字节 short)
 *   Addr 53:     touchtype (1 字节)
 *   
 * @注意：EEPROM 写入寿命约 100 万次，避免频繁调用
 */
void TP_Save_Adjdata(void)
{
    int32_t temp;

    /* 1. 保存校准标志 */
    AT24CXX_WriteOneByte(TP_SAVE_ADDR_BASE, TP_SAVE_FLAG);

    /* 2. 保存 X 轴比例因子 (float 转 int32 存储) */
    temp = (int32_t)(tp_dev.xfac * 100000000.0f);  /* 放大 1 亿倍 */
    AT24CXX_Write(TP_SAVE_ADDR_BASE + 1, (uint8_t*)&temp, 4);

    /* 3. 保存 Y 轴比例因子 */
    temp = (int32_t)(tp_dev.yfac * 100000000.0f);
    AT24CXX_Write(TP_SAVE_ADDR_BASE + 5, (uint8_t*)&temp, 4);

    /* 4. 保存 X 轴偏移量 */
    AT24CXX_Write(TP_SAVE_ADDR_BASE + 9, (uint8_t*)&tp_dev.xoff, 2);

    /* 5. 保存 Y 轴偏移量 */
    AT24CXX_Write(TP_SAVE_ADDR_BASE + 11, (uint8_t*)&tp_dev.yoff, 2);

    /* 6. 保存触摸屏类型 */
    AT24CXX_WriteOneByte(TP_SAVE_ADDR_BASE + 13, tp_dev.touchtype);
}

/**
 * @brief 从 EEPROM 读取校准参数
 * @return 1-成功读取并已校准过，0-未校准过
 * 
 * @读取流程:
 *   1. 检查校准标志是否为 0x0A
 *   2. 依次读取 xfac, yfac, xoff, yoff, touchtype
 *   3. 将 xfac/yfac 从 int32 还原为 float
 *   4. 强制设置为电阻屏模式 (touchtype=1)
 */
u8 TP_Get_Adjdata(void)
{
    int32_t tempfac;

    /* 1. 检查校准标志 */
    if(AT24CXX_ReadOneByte(TP_SAVE_ADDR_BASE) == TP_SAVE_FLAG)
    {
        /* 2. 读取 X 轴比例因子 */
        AT24CXX_Read(TP_SAVE_ADDR_BASE + 1, (uint8_t*)&tempfac, 4);
        tp_dev.xfac = (float)tempfac / 100000000.0f;  /* 还原为 float */

        /* 3. 读取 Y 轴比例因子 */
        AT24CXX_Read(TP_SAVE_ADDR_BASE + 5, (uint8_t*)&tempfac, 4);
        tp_dev.yfac = (float)tempfac / 100000000.0f;

        /* 4. 读取 X 轴偏移量 */
        AT24CXX_Read(TP_SAVE_ADDR_BASE + 9, (uint8_t*)&tp_dev.xoff, 2);
        
        /* 5. 读取 Y 轴偏移量 */
        AT24CXX_Read(TP_SAVE_ADDR_BASE + 11, (uint8_t*)&tp_dev.yoff, 2);

        /* 6. 读取触摸屏类型 */
        tp_dev.touchtype = AT24CXX_ReadOneByte(TP_SAVE_ADDR_BASE + 13);

        /* 7. 强制设置为电阻屏模式 */
        if(tp_dev.touchtype != 1)
        {
            tp_dev.touchtype = 1;  /* 如果不是电阻屏，强制改为电阻屏 */
        }

        return 1;  /* 成功读取 */
    }
    
    return 0;  /* 未找到校准标志 */
}

/* ============================================================================
 * 触摸屏校准程序
 * ============================================================================ */

/**
 * @brief 绘制十字校准点
 * @param x: 中心 X 坐标
 * @param y: 中心 Y 坐标
 * @param color: 线条颜色
 * 
 * @图形样式:
 *      |
 *   ---+---  横线：x-12 到 x+13
 *      |
 *   竖线：y-12 到 y+13
 */
void TP_Drow_Touch_Point(u16 x, u16 y, u16 color)
{
    /* 画横线 */
    LCD_DrawLine_Color(x - 12, y, x + 13, y, color);
    
    /* 画竖线 */
    LCD_DrawLine_Color(x, y - 12, x, y + 13, color);
    
    /* 在四个角画装饰点，让十字更清晰 */
    LCD_DrawFRONT_COLOR(x + 1, y + 1, color);
    LCD_DrawFRONT_COLOR(x - 1, y + 1, color);
    LCD_DrawFRONT_COLOR(x + 1, y - 1, color);
    LCD_DrawFRONT_COLOR(x - 1, y - 1, color);
}

/**
 * @brief 绘制大圆点 (用于画板功能)
 * @param x: X 坐标
 * @param y: Y 坐标
 * @param color: 颜色
 * 
 * @图形样式：5 个点组成的十字星
 *     *
 *   * * *
 *     *
 */
void TP_Draw_Big_Point(u16 x, u16 y, u16 color)
{
    /* 中心点 */
    LCD_DrawFRONT_COLOR(x, y, color);
    
    /* 上下左右四个点 */
    LCD_DrawFRONT_COLOR(x + 1, y, color);
    LCD_DrawFRONT_COLOR(x - 1, y, color);
    LCD_DrawFRONT_COLOR(x, y + 1, color);
    LCD_DrawFRONT_COLOR(x, y - 1, color);
}

/**
 * @brief 触摸屏校准程序
 */
void TP_Adjust(void)
{
    u16 pos_temp[4][2]; // 存储 4 个物理坐标
    u8 cnt = 0;
    u16 d1, d2;
    u32 tem1, tem2;
    double fac;
    u16 outtime = 0;

    cnt = 0;

    LCD_Clear(WHITE);
    FRONT_COLOR = BLACK;
    LCD_ShowString(40, 40, 200, 16, 16, (uint8_t*)"Please touch the cross!");

    // 1. 获取四个点的坐标
    TP_Drow_Touch_Point(20, 20, RED); // 坐标1: 左上角
    tp_dev.sta = 0;
    tp_dev.xfac = 0;
    while(1)
    {
        tp_dev.scan(1); // 物理坐标扫描
        if((tp_dev.sta & 0xc0) == TP_CATH_PRES) // 按下并松开
        {
            outtime = 0;
            tp_dev.sta &= ~(1 << 6); // 清除按键按下标志

            pos_temp[cnt][0] = tp_dev.x[0];
            pos_temp[cnt][1] = tp_dev.y[0];
            cnt++;

            switch(cnt)
            {
                case 1:
                    TP_Drow_Touch_Point(20, 20, WHITE); // 清除坐标1
                    TP_Drow_Touch_Point(tftlcd_data.width - 20, 20, RED); // 坐标2: 右上角
                    break;
                case 2:
                    TP_Drow_Touch_Point(tftlcd_data.width - 20, 20, WHITE); // 清除坐标2
                    TP_Drow_Touch_Point(20, tftlcd_data.height - 20, RED); // 坐标3: 左下角
                    break;
                case 3:
                    TP_Drow_Touch_Point(20, tftlcd_data.height - 20, WHITE); // 清除坐标3
                    TP_Drow_Touch_Point(tftlcd_data.width - 20, tftlcd_data.height - 20, RED); // 坐标4: 右下角
                    break;
                case 4: // 全部点完
                    // 2. 校验点是否合理 (简单判断对角线长度比例)
                    tem1 = abs(pos_temp[0][0] - pos_temp[1][0]); // x1-x2
                    tem2 = abs(pos_temp[0][1] - pos_temp[1][1]); // y1-y2
                    tem1 *= tem1;
                    tem2 *= tem2;
                    d1 = sqrt(tem1 + tem2); // 点1,2之间的距离

                    tem1 = abs(pos_temp[2][0] - pos_temp[3][0]); // x3-x4
                    tem2 = abs(pos_temp[2][1] - pos_temp[3][1]); // y3-y4
                    tem1 *= tem1;
                    tem2 *= tem2;
                    d2 = sqrt(tem1 + tem2); // 点3,4之间的距离

                    fac = (float)d1 / d2;
                    if(fac < 0.95 || fac > 1.05 || d1 == 0 || d2 == 0)
                    {
                        cnt = 0;
                        TP_Drow_Touch_Point(tftlcd_data.width - 20, tftlcd_data.height - 20, WHITE);
                        TP_Drow_Touch_Point(20, 20, RED);
                        continue;
                    }

                    tem1 = abs(pos_temp[0][0] - pos_temp[2][0]); // x1-x3
                    tem2 = abs(pos_temp[0][1] - pos_temp[2][1]); // y1-y3
                    tem1 *= tem1;
                    tem2 *= tem2;
                    d1 = sqrt(tem1 + tem2);

                    tem1 = abs(pos_temp[1][0] - pos_temp[3][0]); // x2-x4
                    tem2 = abs(pos_temp[1][1] - pos_temp[3][1]); // y2-y4
                    tem1 *= tem1;
                    tem2 *= tem2;
                    d2 = sqrt(tem1 + tem2);

                    fac = (float)d1 / d2;
                    if(fac < 0.95 || fac > 1.05)
                    {
                        cnt = 0;
                        TP_Drow_Touch_Point(tftlcd_data.width - 20, tftlcd_data.height - 20, WHITE);
                        TP_Drow_Touch_Point(20, 20, RED);
                        continue;
                    }

                    // 3. 计算校准参数
                    // 以点1(20,20)和点4(width-20, height-20)为基准
                    // 屏幕坐标 = 物理坐标 * fac + off

                    tp_dev.xfac = (float)(tftlcd_data.width - 40) / (pos_temp[3][0] - pos_temp[0][0]);
                    tp_dev.xoff = (tftlcd_data.width - tp_dev.xfac * (pos_temp[3][0] + pos_temp[0][0])) / 2;

                    tp_dev.yfac = (float)(tftlcd_data.height - 40) / (pos_temp[3][1] - pos_temp[0][1]);
                    tp_dev.yoff = (tftlcd_data.height - tp_dev.yfac * (pos_temp[3][1] + pos_temp[0][1])) / 2;

                    if(abs(tp_dev.xfac) > 2 || abs(tp_dev.yfac) > 2)
                    {
                        cnt = 0;
                        TP_Drow_Touch_Point(tftlcd_data.width - 20, tftlcd_data.height - 20, WHITE);
                        TP_Drow_Touch_Point(20, 20, RED);
                        LCD_ShowString(40, 26, 200, 16, 16, (uint8_t*)"Adjust Failed!");
                        tp_delay_ms(1000);
                        LCD_Fill(40, 26, 240, 42, WHITE);
                        continue;
                    }

                    LCD_Clear(WHITE);
                    LCD_ShowString(35, 110, 200, 16, 16, (uint8_t*)"Touch Screen Adjust OK!");
                    tp_delay_ms(1000);
                    TP_Save_Adjdata(); // 保存参数
                    LCD_Clear(WHITE);
                    return; // 校准完成
            }
        }
        tp_delay_ms(10);
        outtime++;
        if(outtime > 1000)
        {
            TP_Get_Adjdata();
            break; // 超时 10 秒退出
        }
    }
}

// ============================================================================
// 主接口函数
// ============================================================================

/**
 * @brief 扫描触摸屏
 * @param tp 0: 获取屏幕坐标 (LCD), 1: 获取物理坐标 (ADC)
 * @return 当前触摸状态 (0:未按下, 非0:按下)
 */
u8 TP_Scan(u8 tp)
{
    if(PEN == 0) // 有按键按下
    {
        if(tp)
        {
            // 获取物理坐标
            TP_Read_XY2(&tp_dev.x[0], &tp_dev.y[0]);
        }
        else if(TP_Read_XY2(&tp_dev.x[0], &tp_dev.y[0]))
        {
            // 物理坐标转换屏幕坐标
            // 注意：这里默认您的屏幕 X, Y 是互换的，并且可能需要翻转
            // 如果点出来的点在对角线方向反了，请修改这里的计算
            tp_dev.x[0] = tp_dev.xfac * tp_dev.x[0] + tp_dev.xoff;
            tp_dev.y[0] = tp_dev.yfac * tp_dev.y[0] + tp_dev.yoff;
        }

        if((tp_dev.sta & TP_PRES_DOWN) == 0)
        {
            tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES; // 标记按下
            tp_dev.x[4] = tp_dev.x[0]; // 备份
            tp_dev.y[4] = tp_dev.y[0];
        }
    }
    else
    {
        if(tp_dev.sta & TP_PRES_DOWN)
        {
            tp_dev.sta &= ~(1 << 7); // 清除按下标志
        }
        else
        {
            tp_dev.x[4] = 0;
            tp_dev.y[4] = 0;
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
        }
    }
    return tp_dev.sta & TP_PRES_DOWN;
}

/**
 * @brief 触摸屏初始化入口函数
 * @return 0: 成功 (已校准), 1: 需要重新校准
 * 
 * @初始化流程:
 *   1. 配置所有相关 GPIO 引脚
 *      - T_PEN(PB1): 上拉输入，检测触摸按下
 *      - T_MISO(PB2): 上拉输入，SPI 数据输出
 *      - T_SCK(PB0): 低速输出，SPI 时钟 (降低速度减小干扰)
 *      - T_CS(PC13): 输出，片选控制
 *      - T_MOSI(PF11): 低速输出，SPI 数据输入
 *   2. 初始化 AT24CXX EEPROM
 *   3. 从 EEPROM 读取校准参数
 *   4. 如果没有校准数据，进入校准模式
 *   
 * @GPIO 速度说明:
 *   T_SCK 和 T_MOSI 使用 LOW 速度，目的是:
 *   - 降低边沿变化率，减小对 FSMC 总线的串扰
 *   - 触摸 SPI 速率本身不高 (约几百 KHz),不需要高速 GPIO
 */
u8 TP_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. 使能所有相关端口的时钟 */
    __HAL_RCC_GPIOB_CLK_ENABLE();   /* T_PEN, T_MISO, T_SCK */
    __HAL_RCC_GPIOC_CLK_ENABLE();   /* T_CS */
    __HAL_RCC_GPIOF_CLK_ENABLE();   /* T_MOSI */

    /* 2. 配置输入引脚：T_PEN(PB1) 和 T_MISO(PB2) 为上拉输入 */
    GPIO_InitStruct.Pin = TOUCH_PEN_PIN | TOUCH_MISO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  /* 上拉，确保无触摸时为高电平 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 3. 配置时钟引脚：T_SCK(PB0) 为推挽输出 */
    GPIO_InitStruct.Pin = TOUCH_SCK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  /* 关键：降低速度减小干扰 */
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TOUCH_SCK_PORT, &GPIO_InitStruct);

    /* 4. 配置片选引脚：T_CS(PC13) 为推挽输出 */
    GPIO_InitStruct.Pin = TOUCH_CS_PIN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOUCH_CS_PORT, &GPIO_InitStruct);

    /* 5. 配置数据输入引脚：T_MOSI(PF11) 为推挽输出 */
    GPIO_InitStruct.Pin = TOUCH_MOSI_PIN;
    HAL_GPIO_Init(TOUCH_MOSI_PORT, &GPIO_InitStruct);

    /* 6. 默认不选中 XPT2046 */
    TCS(1);

    /* 7. 空读一次，稳定 XPT2046 内部状态 */
    TP_Read_XY(&tp_dev.x[0], &tp_dev.y[0]);

    /* 8. 初始化 EEPROM (AT24C02/24C04 等) */
    AT24CXX_Init();

    /* 9. 尝试从 EEPROM 读取校准参数 */
    if(TP_Get_Adjdata())
    {
        return 0;  /* 已经校准过，直接返回成功 */
    }
    else
    {
        /* 未找到校准数据，进入强制校准模式 */
        LCD_Clear(WHITE);
        TP_Adjust();       /* 执行校准程序 */
        TP_Save_Adjdata(); /* 保存校准参数 */
    }
    
    /* 再次读取校准参数，确保生效 */
    TP_Get_Adjdata();

    return 1;  /* 返回 1 表示经过了校准过程 */
}
