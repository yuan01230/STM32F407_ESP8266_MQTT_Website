/**
 * @file    main_drawboard_optimized.c
 * @brief   优化的画板绘图功能代码示例
 * @note    这是优化建议代码，可以替换原 main.c 中的相关部分
 */

/* ============================================================================
 * 优化的画板功能全局变量和宏定义
 * ============================================================================ */

/* 用于记录画板上一个点的坐标 */
static uint16_t g_last_x = 0;
static uint16_t g_last_y = 0;

/* 清屏按钮区域定义 */
#define CLEAR_BTN_X1        (tftlcd_data.width - 60)
#define CLEAR_BTN_Y1        40
#define CLEAR_BTN_X2        (tftlcd_data.width - 10)
#define CLEAR_BTN_Y2        70

/* 画板区域边界 */
#define DRAW_AREA_Y_START   101

/* 触摸防抖时间间隔 (毫秒) */
#define TOUCH_DEBOUNCE_MS   5

/* 坐标显示刷新间隔 (毫秒) */
#define DISP_UPDATE_MS      100

/* 画笔粗细 (像素) */
#define PEN_THICKNESS       3

/* ============================================================================
 * 辅助函数声明
 * ============================================================================ */
void Draw_Clear_Button(void);
void LCD_DrawLine_Thick(u16 x1, u16 y1, u16 x2, u16 y2, u16 color, u8 thickness);

/* ============================================================================
 * 优化的 Draw_Main_UI 函数
 * ============================================================================ */

/**
 * @brief 绘制主界面 UI
 * 
 * 界面布局:
 *   - 顶部标题栏：蓝色背景，白色文字 "Touch Screen Test"
 *   - 状态显示区：显示触摸状态和坐标
 *   - Clear 按钮：右上角红色按钮
 *   - 画板区域：Y > 100 的白色区域
 */
void Draw_Main_UI(void)
{
    /* 1. 清空整个屏幕 */
    LCD_Clear(WHITE);

    /* 2. 绘制顶部标题栏 */
    LCD_Fill(0, 0, tftlcd_data.width, 30, BLUE);   /* 蓝色背景 */
    FRONT_COLOR = WHITE;                            /* 白色文字 */
    LCD_ShowString(10, 7, tftlcd_data.width, 30, 16, (uint8_t*)"Touch Screen Test");
    FRONT_COLOR = BLACK;                            /* 恢复前景色为黑色 */

    /* 3. 绘制状态显示区上边框线 */
    LCD_DrawLine(0, 30, tftlcd_data.width, 30);
    
    /* 4. 显示状态标签和初始值 (预留足够空格方便覆盖) */
    LCD_ShowString(10, 40, tftlcd_data.width, 16, 16, (uint8_t*)"Status : Standby   ");
    LCD_ShowString(10, 60, tftlcd_data.width, 16, 16, (uint8_t*)"Pos X  :        ");
    LCD_ShowString(10, 80, tftlcd_data.width, 16, 16, (uint8_t*)"Pos Y  :        ");

    /* 5. 绘制 Clear 按钮 (位于右上角) */
    Draw_Clear_Button();

    /* 6. 绘制画板区域分隔线 */
    LCD_DrawLine(0, DRAW_AREA_Y_START - 1, tftlcd_data.width, DRAW_AREA_Y_START - 1);
}

/**
 * @brief 绘制 Clear 按钮
 */
void Draw_Clear_Button(void)
{
    LCD_DrawRectangle(CLEAR_BTN_X1, CLEAR_BTN_Y1, CLEAR_BTN_X2, CLEAR_BTN_Y2);
    FRONT_COLOR = RED;
    LCD_ShowString(CLEAR_BTN_X1 + 8, CLEAR_BTN_Y1 + 8, 50, 16, 16, (uint8_t*)"Clear");
    FRONT_COLOR = BLACK;
}

/* ============================================================================
 * 优化的画线函数 (Bresenham 算法 + 可调节粗细)
 * ============================================================================ */

/**
 * @brief 使用 Bresenham 算法绘制粗线
 * @param x1: 起点 X 坐标
 * @param y1: 起点 Y 坐标
 * @param x2: 终点 X 坐标
 * @param y2: 终点 Y 坐标
 * @param color: 线条颜色
 * @param thickness: 线条粗细 (像素)
 */
void LCD_DrawLine_Thick(u16 x1, u16 y1, u16 x2, u16 y2, u16 color, u8 thickness)
{
    s16 dx, dy;
    s16 sx, sy;
    s16 err, e2;
    u8 i;
    
    dx = abs(x2 - x1);
    dy = abs(y2 - y1);
    
    sx = (x1 < x2) ? 1 : -1;
    sy = (y1 < y2) ? 1 : -1;
    
    err = dx - dy;
    
    while(1)
    {
        /* 在每个点画一个厚度为 thickness 的方块 */
        for(i = 0; i < thickness; i++)
        {
            for(u8 j = 0; j < thickness; j++)
            {
                LCD_DrawFRONT_COLOR(x1 + i - thickness/2, y1 + j - thickness/2, color);
            }
        }
        
        if((x1 == x2) && (y1 == y2)) break;
        
        e2 = 2 * err;
        if(e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if(e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

/* ============================================================================
 * 优化的主循环触摸处理代码
 * ============================================================================ */

/**
 * @brief 在主循环中使用的优化触摸处理代码
 * @note 将此代码放入 main.c 的 while(1) 循环中
 */
void Optimized_Touch_Process(void)
{
    static uint32_t last_touch_time = 0;      /* 触摸去抖计时 */
    static uint32_t last_disp_time = 0;       /* 显示更新计时 */
    uint32_t now = HAL_GetTick();
    
    /* ========== 触摸屏扫描与画板功能 ========== */
    
    /* 0: 获取校准后的屏幕坐标 */
    if(tp_dev.scan(0))
    {
        /* 触摸去抖处理 */
        if(now - last_touch_time > TOUCH_DEBOUNCE_MS)
        {
            last_touch_time = now;
            
            /* 1. 更新状态显示 (降低刷新频率) */
            if(now - last_disp_time > DISP_UPDATE_MS)
            {
                FRONT_COLOR = BLUE;
                LCD_ShowString(82, 40, 100, 16, 16, (uint8_t*)"Touching...  ");
                LCD_ShowNum(82, 60, tp_dev.x[0], 4, 16);
                LCD_ShowNum(82, 80, tp_dev.y[0], 4, 16);
                last_disp_time = now;
            }
            
            /* 2. 判断是否点击了 Clear 按钮 */
            if(tp_dev.x[0] >= CLEAR_BTN_X1 && tp_dev.x[0] <= CLEAR_BTN_X2 &&
               tp_dev.y[0] >= CLEAR_BTN_Y1 && tp_dev.y[0] <= CLEAR_BTN_Y2)
            {
                /* 按钮按下反馈 */
                LCD_Fill(CLEAR_BTN_X1, CLEAR_BTN_Y1, CLEAR_BTN_X2, CLEAR_BTN_Y2, GREEN);
                FRONT_COLOR = WHITE;
                LCD_ShowString(CLEAR_BTN_X1 + 8, CLEAR_BTN_Y1 + 8, 50, 16, 16, (uint8_t*)"Clear");
                HAL_Delay(100);  /* 短暂延时让用户看到反馈 */
                
                /* 清空画图区域 */
                LCD_ClearArea(0, DRAW_AREA_Y_START, tftlcd_data.width - 1, tftlcd_data.height - 1);
                
                /* 恢复按钮外观 */
                Draw_Clear_Button();
                
                /* 重置上一次坐标 */
                g_last_x = 0;
                g_last_y = 0;
                
                HAL_Delay(200);  /* 简单防抖 */
            }
            else if(tp_dev.y[0] >= DRAW_AREA_Y_START)  /* 点击在画板区域 */
            {
                if(g_last_x == 0 && g_last_y == 0)
                {
                    /* 如果是第一个点，画一个圆点 */
                    LCD_Draw_Circle(tp_dev.x[0], tp_dev.y[0], PEN_THICKNESS, RED);
                }
                else
                {
                    /* 如果是中间点，用粗线连接 */
                    LCD_DrawLine_Thick(g_last_x, g_last_y, tp_dev.x[0], tp_dev.y[0], RED, PEN_THICKNESS);
                }
                
                /* 更新"上一次坐标" */
                g_last_x = tp_dev.x[0];
                g_last_y = tp_dev.y[0];
            }
        }
    }
    else
    {
        /* 没有触摸时 */
        if(g_last_x != 0 || g_last_y != 0)
        {
            /* 笔抬起了，重置上一次坐标 */
            g_last_x = 0;
            g_last_y = 0;
            
            /* 恢复状态显示 */
            FRONT_COLOR = BLACK;
            LCD_ShowString(82, 40, 100, 16, 16, (uint8_t*)"Standby    ");
            /* 用空格覆盖擦除之前的坐标数字 */
            LCD_ShowString(82, 60, 50, 16, 16, (uint8_t*)"     ");
            LCD_ShowString(82, 80, 50, 16, 16, (uint8_t*)"     ");
        }
    }
}

/* ============================================================================
 * 使用说明
 * ============================================================================ */

/**
 * @brief 如何在 main.c 中使用优化代码
 * 
 * 方法 1: 完全替换
 *   将原 main.c 中的以下部分替换为本文件的对应内容:
 *   - 全局变量定义部分 (第 154-156 行)
 *   - Draw_Main_UI 函数 (第 263-291 行)
 *   - while(1) 循环中的触摸处理部分 (第 167-238 行)
 * 
 * 方法 2: 增量修改
 *   1. 在本文件开头添加全局变量和宏定义到 main.c 的 USER CODE BEGIN 0 区域
 *   2. 复制 Draw_Clear_Button 函数到 main.c 的 USER CODE BEGIN 4 区域
 *   3. 复制 LCD_DrawLine_Thick 函数到 main.c 的 USER CODE BEGIN 4 区域
 *   4. 修改 Draw_Main_UI 函数调用 Draw_Clear_Button
 *   5. 在 while(1) 循环中调用 Optimized_Touch_Process 函数
 * 
 * 主要改进点:
 *   ✅ 添加触摸去抖，防止误触
 *   ✅ 降低坐标显示刷新频率，减少闪烁
 *   ✅ 使用 Bresenham 算法画线，更平滑
 *   ✅ 可调节画笔粗细
 *   ✅ Clear 按钮有按下反馈效果
 *   ✅ 使用宏定义，代码更易维护
 *   ✅ 模块化设计，代码结构更清晰
 */
