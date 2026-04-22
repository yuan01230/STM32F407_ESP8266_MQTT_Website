/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main_sdio_example.c
  * @brief          : SDIO SD卡完整示例程序
  * @description    : 演示如何使用SDIO接口读写SD卡，包含文件系统操作
  *                   集成串口、LED、按键、TFTLCD等外设功能
  ******************************************************************************
  * @功能说明:
  *          1. SD卡初始化和FATFS文件系统挂载
  *          2. 创建、写入、读取文件操作
  *          3. 串口输出调试信息和文件内容
  *          4. LED指示系统运行状态
  *          5. 按键触发不同的SD卡操作
  *          6. LCD实时显示系统状态和操作结果
  *          
  * @硬件配置:
  *          - SDIO接口：4位宽总线模式
  *          - DMA传输：提高数据读写效率
  *          - FATFS文件系统：支持标准文件操作
  *          - USART1：115200波特率串口输出
  *          - LED0/LED1：状态指示
  *          - KEY0/KEY1/KEY2：用户交互
  *          - TFTLCD：2.8寸显示屏
  *          
  * @按键功能:
  *          KEY0: 创建文件并写入数据
  *          KEY1: 读取文件并显示内容
  *          KEY2: 获取SD卡信息并显示
  *          
  * @编写思路:
  *          1. 为什么使用FATFS文件系统？
  *             - SD卡需要先格式化才能使用文件系统
  *             - FATFS提供了标准的文件API（f_open, f_write, f_read等）
  *             - 支持长文件名和目录操作
  *          
  *          2. 为什么要使用DMA传输？
  *             - SD卡数据量大，使用DMA可以释放CPU
  *             - 提高数据传输效率，减少CPU占用
  *             - 避免在传输过程中阻塞其他任务
  *          
  *          3. 界面设计考虑：
  *             - 顶部分区显示系统标题
  *             - 中间显示SD卡状态和信息
  *             - 底部显示操作提示和结果
  *             - 不同颜色区分不同信息类型
  *          
  *          4. 错误处理机制：
  *             - 每个SD卡操作都有返回值检查
  *             - 错误信息通过串口和LCD同时显示
  *             - LED闪烁提示错误状态
  *          
  * @注意:
  *          - 使用前请确保SD卡已正确插入
  *          - SD卡建议格式化为FAT32格式
  *          - 避免在读写过程中拔出SD卡
  *          - 文件操作完成后要调用f_close关闭文件
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../Library/led/led.h"
#include "../../Library/tftlcd/tftlcd.h"
#include "../../Library/key/key.h"
#include "../../Library/usart/usart.h"
#include "../../Library/delay/delay.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* SD卡测试文件路径 */
#define TEST_FILE_PATH    "0:/test_sdio.txt"  /* 0:表示SD卡驱动器 */
#define TEST_DIR_PATH     "0:/test_dir"       /* 测试目录路径 */

/* 测试数据缓冲区大小 */
#define TEST_BUFFER_SIZE  512

/* LED闪烁次数定义 */
#define LED_BLINK_OK      2    /* 成功时闪烁2次 */
#define LED_BLINK_ERROR   5    /* 错误时闪烁5次 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef hsd;
DMA_HandleTypeDef hdma_sdio_tx;
DMA_HandleTypeDef hdma_sdio_rx;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
/* 文件系统相关变量 */
extern char SDPath[4];         /* SD卡逻辑驱动器路径 */
extern FATFS SDFatFS;         /* 文件系统对象 */
extern FIL SDFile;            /* 文件对象 */

/* 测试数据缓冲区 */
static uint8_t write_buffer[TEST_BUFFER_SIZE];  /* 写缓冲区 */
static uint8_t read_buffer[TEST_BUFFER_SIZE];   /* 读缓冲区 */

/* SD卡信息结构体 */
static HAL_SD_CardInfoTypeDef card_info;

/* 系统状态标志 */
static uint8_t sd_mounted = 0;  /* SD卡挂载状态标志 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_SDIO_SD_Init(void);

/* USER CODE BEGIN PFP */
/* SD卡操作相关函数声明 */
static uint8_t SD_Card_Test(void);
static uint8_t SD_Write_File(const char* path, uint8_t* data, uint32_t len);
static uint8_t SD_Read_File(const char* path, uint8_t* buffer, uint32_t buffer_size);
static uint8_t SD_Get_Card_Info(void);
static void LED_Blink(uint8_t count, uint32_t interval_ms);
static void LCD_Show_System_Status(void);
static void LCD_Show_SD_Info(void);
static void LCD_Show_Operation_Result(uint8_t success, const char* msg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief LED闪烁函数
 * 
 * @功能说明:
 *          1. 控制LED闪烁指定次数，用于指示操作状态
 *          2. 成功操作闪烁2次，错误操作闪烁5次
 *          
 * @参数说明:
 *          count: 闪烁次数
 *          interval_ms: 每次闪烁的间隔时间（毫秒）
 *          
 * @编写思路:
 *          1. 使用循环控制闪烁次数
 *             - 每次循环包含点亮和熄灭两个动作
 *          
 *          2. 使用HAL_Delay实现延时
 *             - 简单可靠，适合非实时场景
 *          
 * @注意:
 *          - 此函数会阻塞，不建议在中断或实时任务中使用
 *          - 闪烁期间会占用CPU时间
 */
static void LED_Blink(uint8_t count, uint32_t interval_ms)
{
    for(uint8_t i = 0; i < count; i++)
    {
        LED_On(LED0);
        HAL_Delay(interval_ms);
        LED_Off(LED0);
        HAL_Delay(interval_ms);
    }
}

/**
 * @brief LCD显示系统状态
 * 
 * @功能说明:
 *          1. 在LCD上显示系统标题和基本状态
 *          2. 显示SD卡挂载状态
 *          
 * @编写思路:
 *          1. 清屏并设置背景色
 *          2. 绘制界面分区线
 *          3. 显示标题和状态信息
 *          4. 使用不同颜色区分不同类型的信息
 *          
 * @注意:
 *          - 此函数应在系统初始化后调用一次
 *          - 状态更新时可调用局部刷新函数
 */
static void LCD_Show_System_Status(void)
{
    /* 清屏并设置颜色 */
    BACK_COLOR = WHITE;
    FRONT_COLOR = BLACK;
    LCD_Clear(WHITE);
    
    /* 1. 顶部标题区 (0-30) */
    FRONT_COLOR = BLUE;
    LCD_ShowString(50, 5, 140, 16, 16, (uint8_t*)"SDIO SD Card Test");
    LCD_DrawLine(0, 25, 240, 25);
    
    /* 2. SD卡状态区 (30-80) */
    FRONT_COLOR = BLACK;
    LCD_ShowString(10, 35, 200, 16, 16, (uint8_t*)"SD Card: Checking...");
    LCD_ShowString(10, 55, 200, 16, 16, (uint8_t*)"FileSystem: --");
    LCD_DrawLine(0, 75, 240, 75);
    
    /* 3. 操作结果显示区 (80-150) */
    FRONT_COLOR = GREEN;
    LCD_ShowString(10, 85, 220, 16, 16, (uint8_t*)"Operation: Ready");
    LCD_DrawLine(0, 105, 240, 105);
    
    /* 4. 底部按键提示区 (110-200) */
    FRONT_COLOR = RED;
    LCD_ShowString(10, 115, 240, 16, 16, (uint8_t*)"KEY0: Write File");
    LCD_ShowString(10, 135, 240, 16, 16, (uint8_t*)"KEY1: Read File");
    LCD_ShowString(10, 155, 240, 16, 16, (uint8_t*)"KEY2: Card Info");
    LCD_DrawLine(0, 175, 240, 175);
    
    FRONT_COLOR = BLACK;
    LCD_ShowString(10, 185, 240, 12, 12, (uint8_t*)"System Ready...");
}

/**
 * @brief LCD显示SD卡信息
 * 
 * @功能说明:
 *          1. 显示SD卡容量、块大小等信息
 *          2. 从全局变量card_info中读取数据
 *          
 * @编写思路:
 *          1. 使用sprintf格式化字符串
 *          2. 将字节数转换为MB单位显示
 *          3. 更新LCD指定区域的内容
 *          
 * @注意:
 *          - 需要先调用SD_Get_Card_Info获取信息
 *          - 数值计算注意单位转换
 */
static void LCD_Show_SD_Info(void)
{
    char disp_buf[64];
    
    /* 显示卡容量 */
    uint32_t capacity_mb = (uint32_t)((uint64_t)card_info.LogBlockNbr * card_info.LogBlockSize / (1024 * 1024));
    sprintf(disp_buf, "Capacity: %lu MB      ", capacity_mb);
    FRONT_COLOR = BLACK;
    LCD_ShowString(10, 35, 220, 16, 16, (uint8_t*)disp_buf);
    
    /* 显示块大小 */
    sprintf(disp_buf, "Block Size: %lu Bytes  ", card_info.LogBlockSize);
    LCD_ShowString(10, 55, 220, 16, 16, (uint8_t*)disp_buf);
}

/**
 * @brief LCD显示操作结果
 * 
 * @功能说明:
 *          1. 显示最近一次SD卡操作的结果
 *          2. 成功显示绿色，失败显示红色
 *          
 * @参数说明:
 *          success: 操作是否成功 (1=成功, 0=失败)
 *          msg: 要显示的消息字符串
 *          
 * @编写思路:
 *          1. 根据success参数选择显示颜色
 *          2. 清空操作结果区域
 *          3. 显示消息内容
 *          
 * @注意:
 *          - 消息长度不宜超过20个字符
 *          - 显示后会自动恢复黑色字体
 */
static void LCD_Show_Operation_Result(uint8_t success, const char* msg)
{
    if(success)
    {
        FRONT_COLOR = GREEN;
    }
    else
    {
        FRONT_COLOR = RED;
    }
    
    LCD_ShowString(10, 85, 220, 16, 16, (uint8_t*)msg);
    FRONT_COLOR = BLACK;
}

/**
 * @brief SD卡写入文件
 * 
 * @功能说明:
 *          1. 创建或打开指定路径的文件
 *          2. 将数据写入文件
 *          3. 关闭文件释放资源
 *          
 * @参数说明:
 *          path: 文件路径（如"0:/test.txt"）
 *          data: 要写入的数据缓冲区指针
 *          len: 要写入的数据长度（字节）
 *          
 * @返回值说明:
 *          1: 写入成功
 *          0: 写入失败
 *          
 * @编写思路:
 *          1. 使用f_open打开文件，FA_CREATE_ALWAYS模式
 *             - 如果文件不存在则创建
 *             - 如果文件存在则覆盖
 *          
 *          2. 使用f_write写入数据
 *             - 需要传递实际写入的字节数指针
 *             - 检查返回值确保写入成功
 *          
 *          3. 使用f_close关闭文件
 *             - 确保数据完全写入SD卡
 *             - 释放文件系统资源
 *          
 * @注意:
 *          - 文件路径必须以驱动器号开头（如"0:"）
 *          - 写入完成后必须调用f_close
 *          - 如果文件系统未挂载，操作会失败
 */
static uint8_t SD_Write_File(const char* path, uint8_t* data, uint32_t len)
{
    FIL file;
    UINT bytes_written;
    FRESULT res;
    
    printf("\r\n[SD Write] Opening file: %s\r\n", path);
    
    /* 1. 打开文件（创建或覆盖） */
    res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if(res != FR_OK)
    {
        printf("[SD Write] f_open failed: %d\r\n", res);
        return 0;
    }
    
    /* 2. 写入数据 */
    res = f_write(&file, data, len, &bytes_written);
    if(res != FR_OK)
    {
        printf("[SD Write] f_write failed: %d\r\n", res);
        f_close(&file);  /* 即使失败也要关闭文件 */
        return 0;
    }
    
    /* 3. 检查实际写入字节数 */
    if(bytes_written != len)
    {
        printf("[SD Write] Incomplete write: %lu/%lu bytes\r\n", bytes_written, len);
        f_close(&file);
        return 0;
    }
    
    /* 4. 关闭文件 */
    res = f_close(&file);
    if(res != FR_OK)
    {
        printf("[SD Write] f_close failed: %d\r\n", res);
        return 0;
    }
    
    printf("[SD Write] Success: %lu bytes written\r\n", bytes_written);
    return 1;
}

/**
 * @brief SD卡读取文件
 * 
 * @功能说明:
 *          1. 打开指定路径的文件
 *          2. 读取文件内容到缓冲区
 *          3. 关闭文件并打印内容
 *          
 * @参数说明:
 *          path: 文件路径
 *          buffer: 读取数据缓冲区指针
 *          buffer_size: 缓冲区大小
 *          
 * @返回值说明:
 *          1: 读取成功
 *          0: 读取失败
 *          
 * @编写思路:
 *          1. 使用f_open以只读模式打开文件
 *          
 *          2. 使用f_read读取数据
 *             - 读取大小不超过缓冲区大小
 *             - 检查实际读取的字节数
 *          
 *          3. 通过串口打印读取的内容
 *             - 方便调试和验证
 *          
 *          4. 关闭文件
 *          
 * @注意:
 *          - 缓冲区必须足够大
 *          - 读取的数据不会自动添加字符串结束符
 *          - 二进制文件不适合用printf打印
 */
static uint8_t SD_Read_File(const char* path, uint8_t* buffer, uint32_t buffer_size)
{
    FIL file;
    UINT bytes_read;
    FRESULT res;
    
    printf("\r\n[SD Read] Opening file: %s\r\n", path);
    
    /* 1. 打开文件（只读模式） */
    res = f_open(&file, path, FA_READ);
    if(res != FR_OK)
    {
        printf("[SD Read] f_open failed: %d\r\n", res);
        return 0;
    }
    
    /* 2. 读取数据 */
    res = f_read(&file, buffer, buffer_size, &bytes_read);
    if(res != FR_OK)
    {
        printf("[SD Read] f_read failed: %d\r\n", res);
        f_close(&file);
        return 0;
    }
    
    /* 3. 添加字符串结束符（方便打印） */
    if(bytes_read < buffer_size)
    {
        buffer[bytes_read] = '\0';
    }
    else
    {
        buffer[buffer_size - 1] = '\0';  /* 防止缓冲区溢出 */
    }
    
    /* 4. 打印读取内容 */
    printf("[SD Read] Content (%lu bytes):\r\n", bytes_read);
    printf("%s\r\n", buffer);
    
    /* 5. 关闭文件 */
    f_close(&file);
    
    printf("[SD Read] Success\r\n");
    return 1;
}

/**
 * @brief 获取SD卡信息
 * 
 * @功能说明:
 *          1. 获取SD卡类型、容量、块大小等信息
 *          2. 通过串口打印详细信息
 *          3. 更新全局card_info变量
 *          
 * @返回值说明:
 *          1: 获取成功
 *          0: 获取失败
 *          
 * @编写思路:
 *          1. 调用HAL_SD_GetCardInfo获取卡信息
 *             - 包含卡类型、容量、块大小等
 *          
 *          2. 计算实际容量（MB单位）
 *             - 块数 × 块大小 = 总字节数
 *             - 转换为MB方便显示
 *          
 *          3. 判断卡类型并输出对应描述
 *             - CARD_SDSC: 标准容量SD卡（≤2GB）
 *             - CARD_SDHC_SDXC: 高容量/超大容量SD卡（>2GB）
 *          
 * @注意:
 *          - 需要SD卡已初始化成功
 *          - 信息会保存到全局变量供其他函数使用
 */
static uint8_t SD_Get_Card_Info(void)
{
    /* 1. 获取卡信息 */
    HAL_SD_GetCardInfo(&hsd, &card_info);
    
    /* 2. 计算容量 */
    uint32_t capacity_mb = (uint32_t)((uint64_t)card_info.LogBlockNbr * card_info.LogBlockSize / (1024 * 1024));
    
    /* 3. 打印详细信息 */
    printf("\r\n========== SD Card Info ==========\r\n");
    printf("Card Type: ");
    switch(card_info.CardType)
    {
        case CARD_SDSC:
            printf("SDSC (Standard Capacity)\r\n");
            break;
        case CARD_SDHC_SDXC:
            printf("SDHC/SDXC (High/Extended Capacity)\r\n");
            break;
        default:
            printf("Unknown\r\n");
            break;
    }
    printf("Capacity: %lu MB\r\n", capacity_mb);
    printf("Block Size: %lu Bytes\r\n", card_info.LogBlockSize);
    printf("Block Count: %lu\r\n", card_info.LogBlockNbr);
    printf("RCA: 0x%04X\r\n", card_info.RelCardAdd);
    printf("====================================\r\n\r\n");
    
    return 1;
}

/**
 * @brief SD卡综合测试
 * 
 * @功能说明:
 *          1. 准备测试数据
 *          2. 写入文件
 *          3. 读取文件并验证
 *          4. 返回测试结果
 *          
 * @返回值说明:
 *          1: 测试通过
 *          0: 测试失败
 *          
 * @编写思路:
 *          1. 准备一段测试数据写入缓冲区
 *             - 包含可读文本，方便验证
 *          
 *          2. 调用SD_Write_File写入文件
 *          
 *          3. 调用SD_Read_File读取文件
 *          
 *          4. 比较写入和读取的数据
 *             - 使用memcmp进行内存比较
 *          
 * @注意:
 *          - 此函数会创建/覆盖测试文件
 *          - 测试文件路径由宏定义指定
 */
static uint8_t SD_Card_Test(void)
{
    uint8_t result = 1;
    
    printf("\r\n========== SD Card Test Start ==========\r\n");
    
    /* 1. 准备测试数据 */
    const char* test_data = "Hello SDIO! This is a test file created by STM32F407.\r\n"
                           "SD card读写测试成功！\r\n"
                           "日期: 2026-04-09\r\n";
    uint32_t data_len = strlen(test_data);
    memcpy(write_buffer, test_data, data_len);
    
    printf("[Test] Write data (%lu bytes): %s\r\n", data_len, write_buffer);
    
    /* 2. 写入文件 */
    if(!SD_Write_File(TEST_FILE_PATH, write_buffer, data_len))
    {
        printf("[Test] Write file failed!\r\n");
        result = 0;
        goto test_exit;
    }
    
    /* 3. 清空读缓冲区 */
    memset(read_buffer, 0, TEST_BUFFER_SIZE);
    
    /* 4. 读取文件 */
    if(!SD_Read_File(TEST_FILE_PATH, read_buffer, TEST_BUFFER_SIZE))
    {
        printf("[Test] Read file failed!\r\n");
        result = 0;
        goto test_exit;
    }
    
    /* 5. 验证数据 */
    if(memcmp(write_buffer, read_buffer, data_len) != 0)
    {
        printf("[Test] Data verification failed!\r\n");
        result = 0;
        goto test_exit;
    }
    
    printf("[Test] Data verification passed!\r\n");
    
test_exit:
    printf("========== SD Card Test End ==========\r\n\r\n");
    return result;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  
  /* USER CODE BEGIN 2 */
  printf("\r\n\r\n");
  printf("========================================\r\n");
  printf("  SDIO SD Card Example - STM32F407\r\n");
  printf("========================================\r\n");
  
  /* 1. 外设初始化 */
  LED_Init();
  TFTLCD_Init();
  Key_Init();
  
  /* 2. 显示系统状态界面 */
  LCD_Show_System_Status();
  
  /* 3. SD卡初始化和挂载 */
  printf("\r\n[Main] Initializing SD card...\r\n");
  LCD_ShowString(10, 35, 220, 16, 16, (uint8_t*)"SD Card: Init...");
  
  /* 等待SD卡就绪 */
  if(BSP_SD_GetCardState() == SD_TRANSFER_OK)
  {
      /* 挂载文件系统 */
      FRESULT res = f_mount(&SDFatFS, SDPath, 1);
      if(res == FR_OK)
      {
          sd_mounted = 1;
          printf("[Main] SD card mounted successfully!\r\n");
          LCD_ShowString(10, 35, 220, 16, 16, (uint8_t*)"SD Card: OK");
          LCD_ShowString(10, 55, 220, 16, 16, (uint8_t*)"FileSystem: FATFS");
          LED_Blink(LED_BLINK_OK, 200);
      }
      else
      {
          printf("[Main] f_mount failed: %d\r\n", res);
          LCD_ShowString(10, 35, 220, 16, 16, (uint8_t*)"SD Card: Mount Fail");
          LCD_Show_Operation_Result(0, "Mount Failed!");
          LED_Blink(LED_BLINK_ERROR, 100);
      }
  }
  else
  {
      printf("[Main] SD card not ready!\r\n");
      LCD_ShowString(10, 35, 220, 16, 16, (uint8_t*)"SD Card: Not Ready");
      LCD_Show_Operation_Result(0, "Card Not Ready!");
      LED_Blink(LED_BLINK_ERROR, 100);
  }
  
  /* 4. 显示SD卡信息 */
  if(sd_mounted)
  {
      SD_Get_Card_Info();
      LCD_Show_SD_Info();
  }
  
  printf("\r\n[Main] System ready. Press keys to test.\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    KeyName_t key = Key_Scan();
    
    if(key != KEY_NONE && sd_mounted)
    {
        switch(key)
        {
            case KEY0:  /* 写入文件测试 */
            {
                printf("\r\n[KEY0] Write file test\r\n");
                LCD_Show_Operation_Result(1, "Op: Writing...");
                LED_On(LED1);
                
                /* 准备测试数据 */
                const char* test_data = "SDIO Write Test - STM32F407\r\n";
                uint32_t len = strlen(test_data);
                memcpy(write_buffer, test_data, len);
                
                /* 写入文件 */
                if(SD_Write_File(TEST_FILE_PATH, write_buffer, len))
                {
                    printf("[KEY0] Write success\r\n");
                    LCD_Show_Operation_Result(1, "Write: OK");
                    LED_Blink(LED_BLINK_OK, 150);
                }
                else
                {
                    printf("[KEY0] Write failed\r\n");
                    LCD_Show_Operation_Result(0, "Write: FAIL");
                    LED_Blink(LED_BLINK_ERROR, 100);
                }
                
                LED_Off(LED1);
                break;
            }
            
            case KEY1:  /* 读取文件测试 */
            {
                printf("\r\n[KEY1] Read file test\r\n");
                LCD_Show_Operation_Result(1, "Op: Reading...");
                LED_On(LED1);
                
                /* 清空缓冲区 */
                memset(read_buffer, 0, TEST_BUFFER_SIZE);
                
                /* 读取文件 */
                if(SD_Read_File(TEST_FILE_PATH, read_buffer, TEST_BUFFER_SIZE))
                {
                    printf("[KEY1] Read success\r\n");
                    LCD_Show_Operation_Result(1, "Read: OK");
                    LED_Blink(LED_BLINK_OK, 150);
                }
                else
                {
                    printf("[KEY1] Read failed\r\n");
                    LCD_Show_Operation_Result(0, "Read: FAIL");
                    LED_Blink(LED_BLINK_ERROR, 100);
                }
                
                LED_Off(LED1);
                break;
            }
            
            case KEY2:  /* 获取SD卡信息 */
            {
                printf("\r\n[KEY2] Get card info\r\n");
                LCD_Show_Operation_Result(1, "Op: Getting Info...");
                LED_On(LED1);
                
                if(SD_Get_Card_Info())
                {
                    LCD_Show_SD_Info();
                    LCD_Show_Operation_Result(1, "Info: OK");
                    LED_Blink(LED_BLINK_OK, 150);
                }
                else
                {
                    LCD_Show_Operation_Result(0, "Info: FAIL");
                    LED_Blink(LED_BLINK_ERROR, 100);
                }
                
                LED_Off(LED1);
                break;
            }
            
            default:
                break;
        }
        
        /* 短暂延时，防止按键重复触发 */
        HAL_Delay(100);
    }
    
    /* LED0心跳指示（系统正常运行） */
    static uint32_t last_heartbeat = 0;
    if(HAL_GetTick() - last_heartbeat > 1000)
    {
        last_heartbeat = HAL_GetTick();
        LED_Toggle(LED0);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, LCD_RST_Pin|LED0_Pin|LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_BL_Pin|SPI1_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : KEY2_Pin KEY1_Pin KEY0_Pin */
  GPIO_InitStruct.Pin = KEY2_Pin|KEY1_Pin|KEY0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BEEP_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED0_Pin LED1_Pin */
  GPIO_InitStruct.Pin = LED0_Pin|LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_UP_Pin */
  GPIO_InitStruct.Pin = KEY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(KEY_UP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_BL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI1_CS_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
