# STM32CubeMX 精简工程配置说明

本文档对应当前工程的实际配置目标，只保留这些功能：

- `LED`
- `KEY`
- `Light_Sensor`
- `DHT11`
- `MPU6050`
- `LCD(FSMC)`
- `SDIO + FATFS`
- `SPI1 + EN25Q128`
- `USART1`
- `TIM2`
- `BEEP`

目标不是解释原理，而是把 CubeMX 里每一步怎么点写清楚，方便你重新生成一份和当前代码一致的工程。

---

## 1. 最终外设清单

在 `Pinout & Configuration` 页面，最终应保留这些外设：

- `SYS`
- `RCC`
- `GPIO`
- `DMA`
- `FSMC`
- `SPI1`
- `SDIO`
- `FATFS`
- `ADC3`
- `TIM2`
- `USART1`
- `BEEP`

不应再保留：

- `I2S2`
- `RTC`

如果 CubeMX 里还有它们，先禁用再继续。

---

## 2. 从现有 `.ioc` 开始时，先做的清理

如果你是基于当前 `WIFI.ioc` 继续改，建议先做这一轮清理。

### 2.1 删除外设

在左侧 `Pinout & Configuration`：

1. 展开 `Multimedia`
2. 找到 `I2S2`
3. 设为 `Disable`

然后：

1. 展开 `Timers/RTC`
2. 找到 `RTC`
3. 设为 `Disable`

### 2.2 清理 DMA

进入 `System Core -> DMA`：

- 删除 `SPI2_TX / DMA1_Stream4`

保留：

- `SDIO_RX -> DMA2_Stream3`
- `SDIO_TX -> DMA2_Stream6`

### 2.3 清理中断

进入 `System Core -> NVIC`，取消这些中断：

- `DMA1_Stream4_IRQn`
- `RTC_Alarm_IRQn`
- `RTC_WKUP_IRQn`
- `TAMP_STAMP_IRQn`

保留：

- `SDIO_IRQn`
- `DMA2_Stream3_IRQn`
- `DMA2_Stream6_IRQn`
- `USART1_IRQn`
- `SysTick_IRQn`

---

## 3. 新建工程时的总体配置顺序

推荐按这个顺序配置，返工最少：

1. 选芯片 `STM32F407ZGT6`
2. 配 `SYS`
3. 配 `RCC / Clock Configuration`
4. 配 `GPIO`
5. 配 `FSMC`
6. 配 `SPI1`
7. 配 `ADC3`
8. 配 `USART1`
9. 配 `TIM2`
10. 配 `SDIO`
11. 配 `DMA`
12. 配 `FATFS`
13. 配 `NVIC`
14. 配 `Project Manager`

---

## 4. SYS 配置

进入 `System Core -> SYS`

### 4.1 Debug

- `Debug`: `Serial Wire`

这会保留：

- `PA13 -> SWDIO`
- `PA14 -> SWCLK`

不要选 `Disable`，否则会把调试口关掉。

---

## 5. RCC 与时钟配置

当前工程使用的是：

- `HSI = ON`
- `PLL = ON`
- `SYSCLK = 168MHz`
- 不再依赖 `RTC`，所以不需要 `LSE`
- 不再依赖 `I2S`，所以不需要 `PLLI2S`

### 5.1 RCC 页面设置

进入 `System Core -> RCC`

设置为：

- `High Speed Clock (HSE)`: `Disable`
- `Low Speed Clock (LSE)`: `Disable`
- `Master Clock Output`: `Disable`

保留：

- `HSI`: 默认启用

### 5.2 Clock Configuration 页面设置

进入 `Clock Configuration`

按下面设置：

- `HSI = 16 MHz`
- `PLL Source Mux = HSI`
- `PLLM = 16`
- `PLLN = 336`
- `PLLP = /2`
- `PLLQ = 7`

得到：

- `SYSCLK = 168 MHz`
- `HCLK = 168 MHz`
- `APB1 Prescaler = /4`
- `APB1 = 42 MHz`
- `APB2 Prescaler = /2`
- `APB2 = 84 MHz`

也就是：

- `AHB = 168MHz`
- `APB1 Timer Clock = 84MHz`
- `APB2 Timer Clock = 168MHz`

### 5.3 这套时钟的原因

这套配置和当前代码一致：

- `delay` 基于 `SystemCoreClock`
- `TIM2 Prescaler = 83` 时，计数频率正好是 `84MHz / 84 = 1MHz`
- `SDIO` 的 48MHz 时钟也能由 `PLLQ = 7` 提供

---

## 6. GPIO 与引脚配置

下面是应保留的引脚。

### 6.1 按键

#### `KEY0`

- 引脚：`PE4`
- 模式：`GPIO_Input`
- Pull：`GPIO_PULLUP`
- Label：`KEY0`

#### `KEY1`

- 引脚：`PE3`
- 模式：`GPIO_Input`
- Pull：`GPIO_PULLUP`
- Label：`KEY1`

#### `KEY2`

- 引脚：`PE2`
- 模式：`GPIO_Input`
- Pull：`GPIO_PULLUP`
- Label：`KEY2`

#### `KEY_UP`

- 引脚：`PA0`
- 模式：`GPIO_Input`
- Pull：`GPIO_PULLDOWN`
- Label：`KEY_UP`

### 6.2 LED

#### `LED0`

- 引脚：`PF9`
- 模式：`GPIO_Output`
- Pull：`GPIO_PULLUP`
- Speed：`Medium`
- Label：`LED0`

#### `LED1`

- 引脚：`PF10`
- 模式：`GPIO_Output`
- Pull：`GPIO_PULLUP`
- Speed：`Medium`
- Label：`LED1`

说明：

- 当前板级逻辑是低电平点亮，代码里已经按这个逻辑写好了

### 6.3 LCD 相关普通 GPIO

#### `LCD_RST`

- 引脚：`PF6`
- 模式：`GPIO_Output`
- Pull：`No pull`
- Speed：`Very High`
- Label：`LCD_RST`

#### `LCD_BL`

- 引脚：`PB15`
- 模式：`GPIO_Output`
- Pull：`No pull`
- Speed：`Very High`
- Initial Level：`Set`
- Label：`LCD_BL`

### 6.4 SPI Flash 片选

#### `SPI1_CS`

- 引脚：`PB14`
- 模式：`GPIO_Output`
- Pull：`No pull`
- Speed：`High`
- Initial Level：`Set`
- Label：`SPI1_CS`

### 6.5 BEEP

#### `BEEP`

- 引脚：`PF8`
- 模式：`GPIO_Output`
- Pull：`GPIO_PULLUP`
- Speed：`High`
- Initial Level：`Reset`
- Label：`BEEP`

### 6.6 DHT11

#### `dht11`

- 引脚：`PG9`
- 模式：`GPIO_Output`
- Pull：`GPIO_PULLUP`
- Speed：`High`
- Initial Level：`Set`
- Label：`dht11`

说明：

- DHT11 驱动运行时会在输入/输出模式之间动态切换
- CubeMX 里初始化成输出高电平即可

### 6.7 光敏传感器

#### `Light_Sensor`

- 引脚：`PF7`
- 外设：`ADC3_IN5`
- Label：`Light_Sensor`

---

## 7. FSMC 配置（LCD）

LCD 通过 `FSMC` 驱动，必须保留。

进入 `Connectivity -> FSMC`

### 7.1 选择模式

启用：

- `NOR Flash/PSRAM/SRAM/NAND`

使用当前工程对应的 SRAM/NOR 映射模式，最终生成 `FSMC_NORSRAM_BANK4` 相关初始化。

### 7.2 关键参数

在 `Parameter Settings` 中设置：

- `Address Setup Time = 8`
- `Data Setup Time = 15`
- `Bus Turn Around Duration = 0`
- `Extended Mode = Enable`
- `Extended Data Setup Time = 15`

### 7.3 必须占用的 FSMC 引脚

以下引脚应被 FSMC 占用：

- `PF12 -> FSMC_A6`
- `PE7  -> FSMC_D4`
- `PE8  -> FSMC_D5`
- `PE9  -> FSMC_D6`
- `PE10 -> FSMC_D7`
- `PE11 -> FSMC_D8`
- `PE12 -> FSMC_D9`
- `PE13 -> FSMC_D10`
- `PE14 -> FSMC_D11`
- `PE15 -> FSMC_D12`
- `PD8  -> FSMC_D13`
- `PD9  -> FSMC_D14`
- `PD10 -> FSMC_D15`
- `PD14 -> FSMC_D0`
- `PD15 -> FSMC_D1`
- `PD0  -> FSMC_D2`
- `PD1  -> FSMC_D3`
- `PD4  -> FSMC_NOE`
- `PD5  -> FSMC_NWE`
- `PG12 -> FSMC_NE4`

这些引脚都建议：

- `Pull = No pull`
- `Speed = Very High`

---

## 8. SPI1 配置（EN25Q128）

进入 `Connectivity -> SPI1`

### 8.1 模式

- `Mode = Full-Duplex Master`

### 8.2 引脚

- `PB3 -> SPI1_SCK`
- `PB4 -> SPI1_MISO`
- `PB5 -> SPI1_MOSI`

### 8.3 参数

建议参数：

- `Frame Format = Motorola`
- `Data Size = 8 Bits`
- `First Bit = MSB First`
- `Clock Polarity = Low`
- `Clock Phase = 1 Edge`
- `NSS = Software`

### 8.4 波特率分频

在 CubeMX 里可以先设：

- `Baud Rate Prescaler = 256`

原因：

- 当前原工程就是这个值
- 代码里的 `EN25QXX_Init()` 之后可以动态切换 SPI 速度

如果你后续确认板上 Flash 和走线稳定，也可以在 CubeMX 里直接改成：

- `8`

但建议先按保守值起步。

---

## 9. ADC3 配置（光敏）

进入 `Analog -> ADC3`

### 9.1 通道

- `Regular Conversion Rank 1 = Channel 5`
- 对应引脚：`PF7`

### 9.2 参数

设置：

- `Resolution = 12 bits`
- `Scan Conversion Mode = Disable`
- `Continuous Conversion Mode = Disable`
- `Discontinuous Conversion Mode = Disable`
- `External Trigger Conversion Source = Software Start`
- `External Trigger Conversion Edge = None`
- `Data Alignment = Right`
- `Number Of Conversion = 1`
- `DMA Continuous Requests = Disable`
- `EOC Selection = End of Single Conversion`
- `Sampling Time = 3 Cycles`

### 9.3 不要保留的项

不要再配置：

- `Injected Conversion`
- `Injected Rank`
- `Injected Nbr Of Conversion`
- `Injected Offset`

这部分删干净后，生成代码里也不应该再出现 `HAL_ADCEx_InjectedConfigChannel(...)`。

---

## 10. USART1 配置（调试串口）

进入 `Connectivity -> USART1`

### 10.1 模式

- `Mode = Asynchronous`

### 10.2 引脚

- `PA9  -> USART1_TX`
- `PA10 -> USART1_RX`

### 10.3 参数

设置：

- `Baud Rate = 115200`
- `Word Length = 8 Bits`
- `Parity = None`
- `Stop Bits = 1`
- `Hardware Flow Control = None`
- `Oversampling = 16`

### 10.4 中断

保留：

- `USART1_IRQn`

优先级：

- `Preemption Priority = 4`
- `Sub Priority = 0`

---

## 11. TIM2 配置

进入 `Timers -> TIM2`

### 11.1 时钟源

- `Clock Source = Internal Clock`

### 11.2 参数

设置：

- `Prescaler = 83`
- `Counter Period = 4294967295`
- `Auto Reload Preload = Enable`

说明：

- 在 `APB1 Timer Clock = 84MHz` 时
- `84MHz / (83 + 1) = 1MHz`
- 也就是 `TIM2` 以 `1us` 为单位计数

---

## 12. SDIO 配置（SD 卡）

进入 `Connectivity -> SDIO`

### 12.1 模式

选择：

- `SD 4 bits Wide bus`

### 12.2 引脚

- `PC8  -> SDIO_D0`
- `PC9  -> SDIO_D1`
- `PC10 -> SDIO_D2`
- `PC11 -> SDIO_D3`
- `PC12 -> SDIO_CK`
- `PD2  -> SDIO_CMD`

### 12.3 参数

设置：

- `Clock Edge = Rising`
- `Clock Bypass = Disable`
- `Clock Power Save = Disable`
- `Hardware Flow Control = Disable`
- `Clock Div = 0`

说明：

- 当前代码中 `MX_SDIO_SD_Init()` 初始化时仍是 `1-bit`
- 这是常见做法，后续 `BSP_SD_Init()` / 驱动层会再切到宽总线
- 所以 CubeMX 里选 `4-bit bus` 是对的

---

## 13. DMA 配置（只保留 SDIO）

进入 `System Core -> DMA`

只保留下面两项：

### 13.1 SDIO_RX

- `Instance = DMA2_Stream3`
- `Direction = Peripheral To Memory`
- `Periph Inc = Disable`
- `Mem Inc = Enable`
- `Periph Data Alignment = Word`
- `Mem Data Alignment = Word`
- `Mode = Peripheral Flow Control`
- `Priority = High`
- `FIFO Mode = Enable`
- `FIFO Threshold = Full`
- `Memory Burst = INC4`
- `Peripheral Burst = INC4`

### 13.2 SDIO_TX

- `Instance = DMA2_Stream6`
- `Direction = Memory To Peripheral`
- `Periph Inc = Disable`
- `Mem Inc = Enable`
- `Periph Data Alignment = Word`
- `Mem Data Alignment = Word`
- `Mode = Peripheral Flow Control`
- `Priority = High`
- `FIFO Mode = Enable`
- `FIFO Threshold = Full`
- `Memory Burst = INC4`
- `Peripheral Burst = INC4`

### 13.3 必须删除

- `SPI2_TX / DMA1_Stream4`

---

## 14. FATFS 配置

进入 `Middleware and Software Packs -> FATFS`

### 14.1 绑定存储介质

- 选择 `SD Card`

### 14.2 参数

设置：

- `USE_DMA_TEMPLATE for SD = Enabled`
- `_CODE_PAGE = 936`
- `_USE_LFN = 2`

说明：

- `_CODE_PAGE = 936` 对中文文件名支持更友好
- `_USE_LFN = 2` 支持长文件名，通常够用

---

## 15. NVIC 配置

进入 `System Core -> NVIC`

### 15.1 保留的中断

勾选：

- `SDIO global interrupt`
- `DMA2 Stream3 global interrupt`
- `DMA2 Stream6 global interrupt`
- `USART1 global interrupt`
- `SysTick`

### 15.2 优先级

当前工程实际配置为：

- `SDIO_IRQn = 1,0`
- `DMA2_Stream3_IRQn = 1,0`
- `DMA2_Stream6_IRQn = 1,0`
- `USART1_IRQn = 4,0`
- `SysTick_IRQn = 1,0`

### 15.3 取消勾选

必须取消：

- `DMA1_Stream4_IRQn`
- `RTC_Alarm_IRQn`
- `RTC_WKUP_IRQn`
- `TAMP_STAMP_IRQn`

---

## 16. Project Manager 配置

进入 `Project Manager`

### 16.1 Project

设置：

- `Toolchain / IDE = CMake`
- `Compiler = GCC`

### 16.2 Code Generator

设置：

- `Generate peripheral initialization as a pair of '.c/.h' files per peripheral = 关闭或按你的习惯`
- `Keep User Code when re-generating = Enable`
- `Delete previously generated files when not re-generated = Enable`

### 16.3 高风险点

如果你重新生成代码，以下文件要重点看差异：

- `Core/Src/main.c`
- `Core/Src/stm32f4xx_hal_msp.c`
- `Core/Src/stm32f4xx_it.c`
- `Core/Inc/stm32f4xx_hal_conf.h`

因为 CubeMX 很可能会重新把：

- `HAL_I2S_MODULE_ENABLED`
- `HAL_RTC_MODULE_ENABLED`
- `I2S/RTC 的 MSP 和中断`

又带回来。

---

## 17. 重新生成后你需要人工确认的代码点

### 17.1 `stm32f4xx_hal_conf.h`

应保留：

- `HAL_ADC_MODULE_ENABLED`
- `HAL_SRAM_MODULE_ENABLED`
- `HAL_SD_MODULE_ENABLED`
- `HAL_SPI_MODULE_ENABLED`
- `HAL_TIM_MODULE_ENABLED`
- `HAL_UART_MODULE_ENABLED`
- `HAL_GPIO_MODULE_ENABLED`
- `HAL_DMA_MODULE_ENABLED`

如果你已经彻底删了 `RTC / I2S`，建议这里也不要再启用：

- `HAL_I2S_MODULE_ENABLED`
- `HAL_RTC_MODULE_ENABLED`

### 17.2 `main.c`

初始化顺序应类似：

1. `HAL_Init()`
2. `SystemClock_Config()`
3. `MX_GPIO_Init()`
4. `MX_DMA_Init()`
5. `MX_FSMC_Init()`
6. `MX_SPI1_Init()`
7. `MX_USART1_UART_Init()`
8. `MX_ADC3_Init()`
9. `MX_SDIO_SD_Init()`
10. `MX_FATFS_Init()`
11. `MX_TIM2_Init()`

不应再有：

- `MX_RTC_Init()`
- `MX_I2S2_Init()`
- `HAL_ADCEx_InjectedConfigChannel(...)`

### 17.3 `stm32f4xx_hal_msp.c`

不应再生成：

- `HAL_I2S_MspInit/DeInit`
- `HAL_RTC_MspInit/DeInit`

### 17.4 `stm32f4xx_it.c`

不应再保留：

- `DMA1_Stream4_IRQHandler`
- `RTC_Alarm_IRQHandler`
- `RTC_WKUP_IRQHandler`
- `TAMP_STAMP_IRQHandler`

---

## 18. 当前最终有效链路

最终你在 CubeMX 里应看到的有效链路是：

- `FSMC`：LCD
- `SPI1 + PB14`：EN25Q128
- `SDIO + DMA + FATFS`：SD 卡与中文字库文件
- `ADC3`：光敏
- `GPIO`：按键、LED、BEEP、DHT11
- `USART1`：日志输出
- `TIM2`：保留

---

## 19. 推荐操作方式

最稳妥的做法是：

1. 复制一份当前 `WIFI.ioc`
2. 在 CubeMX 里按本文档清理配置
3. 生成代码
4. 只对比这几个文件：
   - `main.c`
   - `stm32f4xx_hal_msp.c`
   - `stm32f4xx_it.c`
   - `stm32f4xx_hal_conf.h`
5. 确认没有把 `RTC/I2S` 重新带回来
6. 再合并到你当前能编译通过的工程

---

## 20. 一句话结论

如果你只想记住最关键的点，就是这三条：

1. `CubeMX 里删掉 I2S2 和 RTC`
2. `保留 FSMC / SPI1 / ADC3 / USART1 / TIM2 / SDIO / FATFS / DMA`
3. `时钟改成 HSI + PLL = 168MHz，不再依赖 LSE`
