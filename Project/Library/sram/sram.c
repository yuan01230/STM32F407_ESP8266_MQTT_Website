/**
 * @file    sram.c
 * @brief   IS62WV51216 SRAM 驱动程序实现
 * @note    STM32F407 通过 FSMC 接口驱动外部 SRAM
 *          接口类型：FSMC 16 位并行接口
 *          片选 Bank：Bank1 Region3 (基地址 0x68000000)
 * 
 * @note    IS62WV51216 主要特性:
 *          - 容量：512K x 16 bit (1MB = 512KB)
 *          - 工作电压：3.3V
 *          - 访问速度：70ns/100ns
 */

#include "sram.h"
#include <string.h>

/* ============================================================================
   FSMC 句柄定义
   ============================================================================ */

SRAM_HandleTypeDef hsramHandle;
FSMC_NORSRAM_TimingTypeDef sramTiming;

/* ============================================================================
   硬件层函数实现
   ============================================================================ */

/**
 * @brief FSMC GPIO 初始化
 * @note  配置所有 FSMC 相关的 GPIO 引脚为复用推挽输出模式
 * 
 * 实现思路：
 * 1. 使能所有相关 GPIO 端口时钟（GPIOD, GPIOE, GPIOF, GPIOG）
 * 2. 使能 FSMC 外设时钟
 * 3. 按照功能分组配置 GPIO 引脚：
 *    - 控制线：NOE(读使能), NWE(写使能), NBL0/1(字节屏蔽)
 *    - 地址线：A0-A18，分布在 PF, PG, PD 端口
 *    - 数据线：D0-D15，与 TFTLCD 共享，分布在 PD, PE 端口
 * 4. 所有引脚配置为：
 *    - 模式：复用推挽输出 (AF_PP)
 *    - 速度：超高速 (VERY_HIGH) 以满足 SRAM 时序要求
 *    - 复用功能：AF12_FSMC
 *    - 上下拉：无上下拉 (NOPULL)
 */
static void SRAM_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能 GPIO 时钟 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* 使能 FSMC 时钟 */
    __HAL_RCC_FSMC_CLK_ENABLE();

    /* 配置控制线 */
    /* PD4 - FSMC_NOE (OE) */
    /* PD5 - FSMC_NWE (WE) */
    /* PD11~PD13 - FSMC_A16~A18 */
    /* PD14~PD15, PD0~PD1, PD8~PD10 - FSMC_D0~D15 */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 | 
                          GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0 | 
                          GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* PE0~PE1 - FSMC_NBL0, NBL1 */
    /* PE7~PE15 - FSMC_D4~D12 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | 
                          GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | 
                          GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* PF0~PF5, PF12~PF15 - FSMC_A0~A9 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | 
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13 | 
                          GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* PG0~PG5, PG10 - FSMC_A10~A15, NE3 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | 
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_10;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

/**
 * @brief FSMC 时序配置
 * @note  根据 IS62WV51216 的时序参数进行配置
 *        IS62WV51216-70ULI: tAVAV=70ns, tAVQV=70ns, tWC=70ns, tWP=35ns
 * 
 * 实现思路：
 * 1. 计算 FSMC 时钟周期：假设 HCLK=168MHz，则周期≈6ns
 * 2. 配置地址建立时间 (Address Setup Time)：
 *    - SRAM 要求 tAS ≥ 15ns
 *    - 设置 ADDSET=2，实际时间 = (2+1)*6ns = 18ns > 15ns ✓
 * 3. 配置地址保持时间 (Address Hold Time)：
 *    - SRAM 要求 tAH ≥ 0ns
 *    - 但 HAL 库要求 ADDHLD 最小值为 1（数据手册规定）
 *    - 设置 ADDHLD=1，实际时间 = (1+1)*6ns = 12ns ✓
 * 4. 配置数据建立时间 (Data Setup Time)：
 *    - SRAM 要求 tDS ≥ 20ns (写操作)
 *    - 设置 DATAST=4，实际时间 = (4+1)*6ns = 30ns > 20ns ✓
 * 5. 其他参数配置：
 *    - 总线转换周期：0（不需要）
 *    - 访问模式：Mode A（异步 NOR/SRAM 标准模式）
 */
static void SRAM_FSMC_Timing_Config(void)
{
    /* 假设系统时钟为 168MHz (HCLK = 168MHz) */
    /* FSMC 时钟 = HCLK = 168MHz, 周期约 6ns */
    
    /* 地址建立时间 (Address Setup Time) */
    /* IS62WV51216: tAS = 15ns (最小) */
    /* ADDSET = 2 => 3 * 6ns = 18ns > 15ns */
    sramTiming.AddressSetupTime = 2;

    /* 地址保持时间 (Address Hold Time) */
    /* IS62WV51216: tAH = 0ns (最小)
     * 但根据 STM32F4 参考手册，ADDHLD 最小值必须为 1
     * ADDHLD = 1 => 2 * 6ns = 12ns */
    sramTiming.AddressHoldTime = 1;  // 【关键】不能为 0，最小值为 1！

    /* 数据建立时间 (Data Setup Time) */
    /* IS62WV51216: tDS = 20ns (最小，写操作), 但是 tAA (读取) 和 tWC (写入) 周期均为 70ns */
    /* HCLK = 168MHz (6ns/周期) */
    /* (ADDSET+1) + (DATAST+1) = (2+1) + (12+1) = 16 周期 = 96ns > 70ns (安全值) */
    sramTiming.DataSetupTime = 12;

    /* 总线转换周期 (Bus Turn-around Time) */
    sramTiming.BusTurnAroundDuration = 0;

    /* CLK 分频系数 (仅同步模式使用) */
    sramTiming.CLKDivision = 1;

    /* 数据延迟 (仅同步模式使用) */
    sramTiming.DataLatency = 0;

    /* 访问模式 */
    /* Mode A: 异步 NOR/SRAM 标准模式 */
    sramTiming.AccessMode = FSMC_ACCESS_MODE_A;
}

/**
 * @brief FSMC 控制器初始化
 * @note  配置 FSMC Bank1 Region3 用于 SRAM
 * 
 * 实现思路：
 * 1. 配置 FSMC 实例：
 *    - 使用 Bank1 Region3（对应片选信号 NE3）
 *    - 基地址：0x68000000
 * 2. 配置基本参数：
 *    - 数据地址复用：禁用（分离的地址和数据总线）
 *    - 存储器类型：SRAM
 *    - 数据位宽：16 位（匹配 IS62WV51216 的 x16 结构）
 * 3. 配置访问模式：
 *    - 突发访问：禁用（SRAM 不支持突发模式）
 *    - 写操作：使能
 *    - 等待信号：禁用（SRAM 不需要 READY/BUSY 信号）
 *    - 扩展模式：禁用（简化配置，读写时序相同）
 * 4. 调用 HAL_SRAM_Init() 完成初始化
 *    - 传入写时序和读时序参数（两者相同）
 */
static void SRAM_FSMC_Init(void)
{
    /* 配置 FSMC Bank1 Region3 控制寄存器 */
    hsramHandle.Instance = FSMC_NORSRAM_DEVICE;
    hsramHandle.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;

    /* SRAM 控制参数配置 */
    hsramHandle.Init.NSBank = FSMC_NORSRAM_BANK3;              // Bank1 Region3
    hsramHandle.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;  // 数据和地址总线分离
    hsramHandle.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;       // SRAM 类型
    hsramHandle.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;  // 16 位数据宽度
    hsramHandle.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;    // 禁用突发访问
    hsramHandle.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    hsramHandle.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    hsramHandle.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    
    hsramHandle.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;      // 使能写操作
    hsramHandle.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;             // 禁用等待信号
    hsramHandle.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;         // 禁用扩展模式 (读写时序相同)
    hsramHandle.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    hsramHandle.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;             // 禁用写突发
    hsramHandle.Init.ContinuousClock = FSMC_CONTINUOUS_CLOCK_SYNC_ONLY; // 仅同步模式使用连续时钟

    /* 初始化时序参数 */
    SRAM_FSMC_Timing_Config();

    /* 初始化 FSMC */
    if (HAL_SRAM_Init(&hsramHandle, &sramTiming, &sramTiming) != HAL_OK)
    {
        /* 初始化失败处理 */
        Error_Handler();
    }
}

/* ============================================================================
   应用层函数实现
   ============================================================================ */

/**
 * @brief SRAM 初始化
 * @note  依次初始化 GPIO 和 FSMC
 * 
 * 实现思路：
 * 1. 先初始化 GPIO：配置所有 FSMC 相关引脚
 * 2. 再初始化 FSMC 控制器：配置时序和工作模式
 * 3. 初始化完成后，SRAM 即可通过直接指针访问
 */
void SRAM_Init(void)
{
    SRAM_GPIO_Init();
    SRAM_FSMC_Init();
}

/**
 * @brief 向 SRAM 写入一个字节
 * @param addr: SRAM 地址 (0 ~ 524287)
 * @param data: 要写入的数据
 * 
 * 实现思路：
 * 1. 边界检查：确保地址在有效范围内 (0 ~ 512KB-1)
 * 2. 地址转换：由于 FSMC 配置为 16 位数据宽度，需要将字节地址转换为 16 位字地址
 *    - halfWordAddr = addr / 2
 * 3. 读取当前 16 位值：因为只能写 16 位，需要先读出整个字
 * 4. 根据地址奇偶性决定保留哪部分：
 *    - 偶数地址 (addr%2==0)：保留高 8 位，替换低 8 位
 *    - 奇数地址 (addr%2==1)：保留低 8 位，替换高 8 位
 * 5. 将修改后的 16 位值写回 SRAM
 * 
 * 注意：这种方法会进行"读 - 改 - 写"操作，不是原子操作
 */
void SRAM_WriteByte(uint32_t addr, uint8_t data)
{
    /* 边界检查 */
    if (addr >= SRAM_SIZE_BYTES)
        return;

    /* 
     * 优化：利用 STM32 FSMC 的 NBL0/NBL1 硬件屏蔽信号
     * 直接通过 8 位指针访问，硬件会自动根据地址奇偶性控制字节使能引脚
     * 效率更高且满足线程安全
     */
    *(uint8_t *)(SRAM_BASE_ADDR + addr) = data;
}

/**
 * @brief 从 SRAM 读取一个字节
 * @param addr: SRAM 地址 (0 ~ 524287)
 * @return 读取到的数据
 * 
 * 实现思路：
 * 1. 边界检查：确保地址在有效范围内
 * 2. 地址转换：将字节地址转换为 16 位字地址
 *    - halfWordAddr = addr / 2
 * 3. 从 SRAM 读取 16 位数据
 * 4. 根据地址奇偶性提取对应的 8 位：
 *    - 偶数地址 (addr%2==0)：返回低 8 位 (val & 0x00FF)
 *    - 奇数地址 (addr%2==1)：返回高 8 位 (val >> 8)
 * 
 * 注意：FSMC 会自动处理片选和时序，软件只需像访问内存一样读取
 */
uint8_t SRAM_ReadByte(uint32_t addr)
{
    /* 边界检查 */
    if (addr >= SRAM_SIZE_BYTES)
        return 0;

    /* 优化：直接通过 8 位指针读取 */
    return *(uint8_t *)(SRAM_BASE_ADDR + addr);
}

/**
 * @brief 向 SRAM 写入一个 16 位字
 * @param addr: SRAM 地址 (0 ~ 262143)
 * @param data: 要写入的数据 (16 位)
 * 
 * 实现思路：
 * 1. 边界检查：确保地址在有效范围内 (0 ~ 256K-1，因为是 16 位访问)
 * 2. 直接通过指针写入 16 位数据：
 *    - 基地址 + 地址偏移*2（因为 16 位 = 2 字节）
 *    - 强制类型转换为 uint16_t* 指针
 * 3. FSMC 硬件自动完成：
 *    - 片选信号有效 (CS)
 *    - 地址锁存
 *    - 数据输出到数据总线
 *    - 写使能有效 (WE)
 * 
 * 优点：直接访问，无需读 - 改 - 写，效率高
 */
void SRAM_WriteHalfWord(uint32_t addr, uint16_t data)
{
    /* 边界检查 */
    if (addr >= (SRAM_SIZE_BYTES / 2))
        return;

    /* 直接写入 16 位数据 */
    *(uint16_t *)(SRAM_BASE_ADDR + addr * 2) = data;
}

/**
 * @brief 从 SRAM 读取一个 16 位字
 * @param addr: SRAM 地址 (0 ~ 262143)
 * @return 读取到的数据 (16 位)
 * 
 * 实现思路：
 * 1. 边界检查：确保地址在有效范围内
 * 2. 直接通过指针读取 16 位数据：
 *    - 基地址 + 地址偏移*2
 *    - 强制类型转换为 uint16_t* 指针
 * 3. FSMC 硬件自动完成：
 *    - 片选信号有效 (CS)
 *    - 地址锁存
 *    - 读使能有效 (OE)
 *    - 从数据总线读取数据
 * 
 * 优点：直接访问，单次操作即可完成，效率高
 */
uint16_t SRAM_ReadHalfWord(uint32_t addr)
{
    /* 边界检查 */
    if (addr >= (SRAM_SIZE_BYTES / 2))
        return 0;

    /* 直接读取 16 位数据 */
    return *(uint16_t *)(SRAM_BASE_ADDR + addr * 2);
}

/**
 * @brief 向 SRAM 连续写入多个字节
 * @param addr: 起始地址
 * @param pBuf: 数据缓冲区指针
 * @param len: 要写入的字节数
 * 
 * 实现思路：
 * 1. 边界检查：确保起始地址 + 长度不超过 SRAM 总容量
 * 2. 使用循环逐个字节写入：
 *    - 从起始地址开始
 *    - 每次调用 SRAM_WriteByte()
 *    - 地址递增
 * 3. 优点：简单可靠，支持任意起始地址（包括奇数地址）
 * 4. 缺点：效率较低，大量数据时建议使用 DMA 或 16 位批量写入
 */
void SRAM_WriteBuffer(uint32_t addr, uint8_t *pBuf, uint32_t len)
{
    uint32_t i;
    
    /* 边界检查 */
    if ((addr + len) > SRAM_SIZE_BYTES)
        return;

    for (i = 0; i < len; i++)
    {
        SRAM_WriteByte(addr + i, pBuf[i]);
    }
}

/**
 * @brief 从 SRAM 连续读取多个字节
 * @param addr: 起始地址
 * @param pBuf: 数据缓冲区指针
 * @param len: 要读取的字节数
 * 
 * 实现思路：
 * 1. 边界检查：确保起始地址 + 长度不超过 SRAM 总容量
 * 2. 使用循环逐个字节读取：
 *    - 从起始地址开始
 *    - 每次调用 SRAM_ReadByte()
 *    - 地址递增，数据存入缓冲区
 * 3. 优点：简单可靠，支持任意起始地址
 * 4. 缺点：效率较低，大量数据时建议使用 DMA 或 16 位批量读取
 */
void SRAM_ReadBuffer(uint32_t addr, uint8_t *pBuf, uint32_t len)
{
    uint32_t i;
    
    /* 边界检查 */
    if ((addr + len) > SRAM_SIZE_BYTES)
        return;

    for (i = 0; i < len; i++)
    {
        pBuf[i] = SRAM_ReadByte(addr + i);
    }
}

/**
 * @brief 填充 SRAM 指定区域
 * @param addr: 起始地址
 * @param data: 要填充的数据
 * @param len: 填充长度 (字节数)
 * 
 * 实现思路：
 * 1. 边界检查：确保起始地址 + 长度不超过 SRAM 总容量
 * 2. 使用循环逐字节写入相同数据：
 *    - 从起始地址开始
 *    - 每次调用 SRAM_WriteByte()
 *    - 写入相同的 data 值
 * 3. 典型应用：
 *    - 内存清零：SRAM_Fill(addr, 0x00, len)
 *    - 内存填充：SRAM_Fill(addr, 0xFF, len)
 *    - 图案填充：用于 LCD 帧缓冲初始化
 */
void SRAM_Fill(uint32_t addr, uint8_t data, uint32_t len)
{
    uint32_t i;
    
    /* 边界检查 */
    if ((addr + len) > SRAM_SIZE_BYTES)
        return;

    for (i = 0; i < len; i++)
    {
        SRAM_WriteByte(addr + i, data);
    }
}

/**
 * @brief SRAM 自检函数
 * @retval 0: 成功; 其他：失败
 * 
 * 实现思路：
 * 1. 选择 3 个测试点覆盖整个地址空间：
 *    - 起始位置：地址 0
 *    - 中间位置：地址 1024
 *    - 结束位置：最后一个地址
 * 2. 使用 3 种不同的测试数据：
 *    - 0x55AA (01010101 10101010)：交替的 01  pattern
 *    - 0xAA55 (10101010 01010101)：反转的交替 pattern
 *    - 0x1234：普通数值
 * 3. 对每个测试点执行：
 *    - 写入测试数据
 *    - 短暂延时确保写入完成
 *    - 读回数据并比较
 *    - 不一致则返回错误码 1
 * 4. 额外测试：全 0 和全 F 测试
 *    - 测试存储单元能否稳定保持极端值
 *    - 失败分别返回错误码 2 和 3
 * 5. 恢复初始值并返回成功
 * 
 * 注意：此测试会破坏原有数据，使用前请确保重要数据已备份
 */
uint8_t SRAM_SelfTest(void)
{
    /* 
     * 修改点：将 testAddr 修改为 16-bit 字索引
     * 因为 SRAM_WriteHalfWord 期望的是 16 位字的偏移量
     */
    uint32_t testAddr[3] = {0, 1024, (SRAM_SIZE_BYTES / 2) - 1};
    uint16_t testData[3] = {0x55AA, 0xAA55, 0x1234};
    uint32_t i;
    
    /* 测试写入和读取 */
    for (i = 0; i < 3; i++)
    {
        /* 写入测试数据 */
        SRAM_WriteHalfWord(testAddr[i], testData[i]);
        
        /* 延时以确保信号稳定 */
        HAL_Delay(1);
        
        /* 读回数据并比较 */
        if (SRAM_ReadHalfWord(testAddr[i]) != testData[i])
        {
            return 1;  /* 测试失败 (地址越界或时序错误) */
        }
    }
    
    /* 额外测试：全 0 和全 F 测试 */
    SRAM_WriteHalfWord(0, 0x0000);
    HAL_Delay(1);
    if (SRAM_ReadHalfWord(0) != 0x0000)
        return 2;
    
    SRAM_WriteHalfWord(0, 0xFFFF);
    HAL_Delay(1);
    if (SRAM_ReadHalfWord(0) != 0xFFFF)
        return 3;
    
    /* 恢复初始值 */
    SRAM_WriteHalfWord(0, 0x55AA);
    
    return 0;  /* 测试成功 */
}
