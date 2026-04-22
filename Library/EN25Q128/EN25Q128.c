//
// Created by 标语 on 26-1-2.
//

#include "EN25Q128.h"
#include "stdio.h"
#include "stm32f4xx.h"

uint16_t EN25QXX_TYPE = EN25Q128;	//默认是EN25Q128
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
/**
 * @brief  设置SPI1通信速度
 * @note   SPI速度 = fAPB1 / 分频系数，fAPB1时钟一般为42MHz
 * @param  SPI_BaudRatePrescaler: SPI波特率预分频值
 *         可选值: SPI_BAUDRATEPRESCALER_2 ~ SPI_BAUDRATEPRESCALER_256
 * @retval 无
 * @optimization 建议:
 *         1. 添加返回值指示设置成功/失败
 *         2. 添加互斥保护，避免SPI传输过程中修改速度
 */
void SPI1_SetSpeed(uint8_t SPI_BaudRatePrescaler)
{
	assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));//判断有效性
	__HAL_SPI_DISABLE(&hspi1); //关闭SPI
	hspi1.Instance->CR1&=0XFFC7; //位3-5 清零，用来设置波特率
	hspi1.Instance->CR1|=SPI_BaudRatePrescaler;//设置SPI 速度
	__HAL_SPI_ENABLE(&hspi1); //使能SPI
}

/**
 * @brief  初始化EN25QXX Flash芯片
 * @note   EN25Q128规格:
 *         - 容量: 16MB (128Mbit)
 *         - Sector: 4KB，共4096个
 *         - Block: 64KB (16个Sector)，共256个
 * @param  无
 * @retval 无
 * @optimization 建议:
 *         1. 添加返回值检查ID是否匹配预期型号
 *         2. 添加初始化失败处理机制
 *         3. 可选配置SPI速度参数
 */
void EN25QXX_Init(void)
{
	//片选拉高，取消选中Flash芯片
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,SET);

	// SPI1_SetSpeed(SPI_BAUDRATEPRESCALER_2);
	EN25QXX_TYPE=EN25QXX_ReadID();	//读取FLASH ID.
	// printf("EN25QXX_TYPE=%X\r\n",EN25QXX_TYPE);

}

/**
 * @brief  读取EN25QXX的状态寄存器
 * @note   状态寄存器位定义:
 *         BIT7  6   5   4   3   2   1   0
 *         SPR   RV  TB BP2 BP1 BP0 WEL BUSY
 *         - SPR: 状态寄存器保护位(配合WP引脚使用)
 *         - TB,BP2,BP1,BP0: Flash区域写保护设置
 *         - WEL: 写使能锁定位(1=写使能)
 *         - BUSY: 忙标记位(1=忙, 0=空闲)
 *         默认值: 0x00
 * @param  无
 * @retval 状态寄存器的值
 */
uint8_t EN25QXX_ReadSR(void)
{
	uint8_t tx_buf[2] = {EN25X_ReadStatusReg, 0xFF};
	uint8_t rx_buf[2];
	
	// 片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 使用TransmitReceive一次性完成读取
	HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 2, HAL_MAX_DELAY);
	
	// 片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	return rx_buf[1];  // 返回实际读取的状态寄存器值
}

/**
 * @brief  写EN25QXX状态寄存器
 * @note   只有SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)可以写
 *         写状态寄存器前需要先发送写使能命令
 * @param  sr: 要写入状态寄存器的值
 * @retval 无
 */
void EN25QXX_Write_SR(uint8_t sr)
{
	uint8_t cmd = EN25X_WriteStatusReg;
	
	// 先使能写操作
	EN25QXX_Write_Enable();
	
	// 片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送写状态寄存器命令
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 写入状态寄存器值
	HAL_SPI_Transmit(&hspi1, &sr, 1, HAL_MAX_DELAY);
	
	// 片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// 等待写入完成
	EN25QXX_Wait_Busy();
}

/**
 * @brief  EN25QXX写使能命令
 * @note   在执行写操作(页编程、扇区擦除、芯片擦除)前必须调用
 *         该命令会将状态寄存器的WEL位置1
 * @param  无
 * @retval 无
 * @optimization 建议:
 *         1. 添加WEL位设置成功的验证(读状态寄存器检查)
 *         2. 添加返回值表示是否设置成功
 */
void EN25QXX_Write_Enable(void)
{
	uint8_t spi_txbyte = 0;
	spi_txbyte = EN25X_WriteEnable;
	//片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,RESET); //使能器件
	HAL_SPI_Transmit(&hspi1,(uint8_t *)(&(spi_txbyte)),1,HAL_MAX_DELAY);//发送写使能命令
	//片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,SET);
}

/**
 * @brief  EN25QXX写禁止命令
 * @note   该命令会将状态寄存器的WEL位清零，禁止写操作
 *         一般在写操作完成后，Flash会自动清除WEL位
 * @param  无
 * @retval 无
 * @optimization 建议:
 *         1. 此函数在实际应用中较少使用，Flash会自动清除WEL
 *         2. 可添加WEL位清除成功的验证
 */
void EN25QXX_Write_Disable(void)
{
	uint8_t spi_txbyte = 0;
	spi_txbyte = EN25X_WriteDisable;
	//片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,RESET); //使能器件
	HAL_SPI_Transmit(&hspi1,(uint8_t *)(&(spi_txbyte)),1,HAL_MAX_DELAY);//发送写禁止命令
	//片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port,SPI1_CS_Pin,SET);
}

/**
 * @brief  读取Flash芯片ID
 * @note   使用命令0x90(制造商/设备ID命令)读取
 *         不同型号返回值:
 *         - 0xEF13: EN25Q80
 *         - 0xEF14: EN25Q16
 *         - 0xEF15: EN25Q32
 *         - 0xEF16: EN25Q64
 *         - 0xEF17: EN25Q128
 * @param  无
 * @retval Flash芯片ID (16位)
 * @optimization 建议:
 *         1. 可使用JEDEC ID命令(0x9F)获取更完整的信息
 *         2. 添加错误处理，如读取失败返回0xFFFF
 *         3. 注释中说明实际使用的芯片ID值(0x6817)
 */
uint16_t EN25QXX_ReadID(void)
{
	uint16_t Temp = 0;

	uint8_t tx_buf[8] = {0x90, 0x00, 0x00, 0x00, 0x00, 0x00}; // 后两个0x00只是提供时钟
	uint8_t rx_buf[8];

	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);

	HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 6, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

	// HAL_UART_Transmit(&huart1, rx_buf, 8, HAL_MAX_DELAY);
	// printf("Temp:%x\r\n",Temp);
	Temp = rx_buf[4]<<8|rx_buf[5];
	return Temp;
}

/**
 * @brief  从SPI Flash读取数据
 * @note   可以从任意地址开始读取，没有页边界限制
 *         读取前不需要擦除操作
 * @param  pBuffer: 数据存储缓冲区指针
 * @param  ReadAddr: 读取起始地址(24位地址，0x000000~0xFFFFFF)
 * @param  NumByteToRead: 要读取的字节数(最多65535)
 * @retval 无
 */
void EN25QXX_Read(uint8_t* pBuffer,uint32_t ReadAddr,uint16_t NumByteToRead)
{
	uint8_t cmd = EN25X_ReadData;
	uint8_t addr[3];
	
	// 参数有效性检查
	if(pBuffer == NULL || NumByteToRead == 0) return;
	
	// 地址字节拆分（高字节在前，MSB first）
	addr[0] = (ReadAddr >> 16) & 0xFF;  // A23-A16
	addr[1] = (ReadAddr >> 8) & 0xFF;   // A15-A8
	addr[2] = ReadAddr & 0xFF;          // A7-A0
	
	// 片选拉低，开始通信
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送读取命令
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 发送24位地址（分三次发送，确保字节序正确）
	HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
	
	// 一次性接收所有数据（优化性能）
	HAL_SPI_Receive(&hspi1, pBuffer, NumByteToRead, HAL_MAX_DELAY);
	
	// 片选拉高，结束通信
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
}

/**
 * @brief  在Flash的一页内写入数据
 * @note   Flash页大小为256字节，不能跨页写入
 *         写入前必须确保地址范围内的数据全部为0xFF
 * @param  pBuffer: 数据存储缓冲区指针
 * @param  WriteAddr: 写入起始地址(24位地址)
 * @param  NumByteToWrite: 要写入的字节数(最多256)，不应超过该页剩余字节数
 * @retval 无
 */
void EN25QXX_Write_Page(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	uint8_t cmd = EN25X_PageProgram;
	uint8_t addr[3];
	
	// 参数有效性检查
	if(pBuffer == NULL || NumByteToWrite == 0 || NumByteToWrite > 256) return;
	
	// 地址字节拆分（高字节在前，MSB first）
	addr[0] = (WriteAddr >> 16) & 0xFF;  // A23-A16
	addr[1] = (WriteAddr >> 8) & 0xFF;   // A15-A8
	addr[2] = WriteAddr & 0xFF;          // A7-A0
	
	// 先使能写操作
	EN25QXX_Write_Enable();
	
	// 片选拉低，开始通信
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送页编程命令
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 发送24位地址
	HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
	
	// 一次性发送所有数据（优化性能）
	HAL_SPI_Transmit(&hspi1, pBuffer, NumByteToWrite, HAL_MAX_DELAY);
	
	// 片选拉高，结束通信
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// 等待写入完成
	EN25QXX_Wait_Busy();
}

/**
 * @brief  在Flash任意地址写入数据(无校验)
 * @note   必须确保所写的地址范围内的数据全部为0xFF，否则写入失败
 *         具有自动换页功能，可跨页写入
 * @param  pBuffer: 数据存储缓冲区指针
 * @param  WriteAddr: 写入起始地址(24位地址)
 * @param  NumByteToWrite: 要写入的字节数(最多65535)
 * @retval 无
 */
void EN25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	uint16_t pageremain;
	
	// 参数有效性检查
	if(pBuffer == NULL || NumByteToWrite == 0) return;
	
	// 计算当前页剩余空间
	pageremain = 256 - (WriteAddr % 256);
	if(NumByteToWrite <= pageremain) {
		pageremain = NumByteToWrite;
	}
	
	while(1)
	{
		// 写入当前页
		EN25QXX_Write_Page(pBuffer, WriteAddr, pageremain);
		
		if(NumByteToWrite == pageremain) {
			break;  // 写入完成
		}
		else {
			// 还有数据需要写入
			pBuffer += pageremain;
			WriteAddr += pageremain;
			NumByteToWrite -= pageremain;
			
			// 计算下一页写入数据量
			if(NumByteToWrite > 256) {
				pageremain = 256;
			} else {
				pageremain = NumByteToWrite;
			}
		}
	}
}

/**
 * @brief  在Flash任意地址写入数据(带擦除操作)
 * @note   该函数会自动检查目标区域是否需要擦除
 *         如果需要擦除，会先读取整个扇区，擦除后再写入新数据
 *         具有自动跨扇区写入功能
 * @param  pBuffer: 数据存储缓冲区指针
 * @param  WriteAddr: 写入起始地址(24位地址)
 * @param  NumByteToWrite: 要写入的字节数(最多65535)
 * @retval 无
 */
uint8_t EN25QXX_BUFFER[4096];
void EN25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
	uint32_t secpos;
	uint16_t secoff;
	uint16_t secremain;
 	uint16_t i;
	uint8_t * EN25QXX_BUF;
	
	// 参数有效性检查
	if(pBuffer == NULL || NumByteToWrite == 0) return;
	
   	EN25QXX_BUF = EN25QXX_BUFFER;
 	secpos = WriteAddr / 4096;         // 扇区编号
	secoff = WriteAddr % 4096;         // 在扇区内的偏移
	secremain = 4096 - secoff;         // 扇区剩余空间
	
 	if(NumByteToWrite <= secremain) {
		secremain = NumByteToWrite;
	}
	
	while(1)
	{
		// 读取整个扇区的内容
		EN25QXX_Read(EN25QXX_BUF, secpos * 4096, 4096);
		
		// 检查是否需要擦除
		for(i = 0; i < secremain; i++) {
			if(EN25QXX_BUF[secoff + i] != 0xFF) {
				break;  // 发现非0xFF，需要擦除
			}
		}
		
		if(i < secremain) {
			// 需要擦除
			EN25QXX_Erase_Sector(secpos);
			
			// 复制新数据到缓冲区
			for(i = 0; i < secremain; i++) {
				EN25QXX_BUF[i + secoff] = pBuffer[i];
			}
			
			// 写入整个扇区
			EN25QXX_Write_NoCheck(EN25QXX_BUF, secpos * 4096, 4096);
		} else {
			// 不需要擦除，直接写入
			EN25QXX_Write_NoCheck(pBuffer, WriteAddr, secremain);
		}
		
		if(NumByteToWrite == secremain) {
			break;  // 写入完成
		}
		else {
			// 还有数据需要写入
			secpos++;                      // 下一个扇区
			secoff = 0;                    // 从扇区开头开始写
		   	pBuffer += secremain;
			WriteAddr += secremain;
		   	NumByteToWrite -= secremain;
			
			if(NumByteToWrite > 4096) {
				secremain = 4096;
			} else {
				secremain = NumByteToWrite;
			}
		}
	}
}

/**
 * @brief  擦除整个Flash芯片
 * @note   擦除时间非常长，根据芯片型号不同，可能需要几分钟
 *         擦除后所有数据变为0xFF
 * @param  无
 * @retval 无
 */
void EN25QXX_Erase_Chip(void)
{
	uint8_t cmd = EN25X_ChipErase;
	
	// 使能写操作
	EN25QXX_Write_Enable();
	
	// 片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送芯片擦除命令
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// 等待芯片擦除结束
	EN25QXX_Wait_Busy();
}

/**
 * @brief  擦除Flash的一个扇区(4KB)
 * @note   扇区大小为4096字节(4KB)
 *         擦除一个扇区的时间约150ms
 * @param  Dst_Addr: 扇区编号(0~4095)，会自动乘4096
 * @retval 无
 */
void EN25QXX_Erase_Sector(uint32_t Dst_Addr)
{
	uint8_t cmd = EN25X_SectorErase;
	uint8_t addr[3];
	uint32_t sector_addr;
	
	// 计算实际地址（扇区编号 * 4096）
	sector_addr = Dst_Addr * 4096;
	
	// 地址字节拆分（高字节在前，MSB first）
	addr[0] = (sector_addr >> 16) & 0xFF;  // A23-A16
	addr[1] = (sector_addr >> 8) & 0xFF;   // A15-A8
	addr[2] = sector_addr & 0xFF;          // A7-A0
	
	// 先使能写操作
	EN25QXX_Write_Enable();
	
	// 片选拉低，开始通信
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送扇区擦除命令
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 发送24位地址
	HAL_SPI_Transmit(&hspi1, addr, 3, HAL_MAX_DELAY);
	
	// 片选拉高，结束通信
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// 等待擦除完成
	EN25QXX_Wait_Busy();
}

/**
 * @brief  等待Flash空闲
 * @note   通过轮询状态寄存器的BUSY位(bit 0)来判断
 *         BUSY=1表示Flash忙，BUSY=0表示空闲
 *         已添加超时保护，防止硬件异常导致死循环
 * @param  无
 * @retval 无
 */
void EN25QXX_Wait_Busy(void)
{
	uint32_t timeout = 0xFFFFFF;  // 超时计数器
	
	while((EN25QXX_ReadSR() & 0x01) == 0x01) {
		if(--timeout == 0) {
			// 超时，退出等待（可添加错误处理）
			// printf("EN25QXX_Wait_Busy timeout!\r\n");
			return;
		}
	}
}

/**
 * @brief  使Flash芯片进入掉电模式
 * @note   掉电模式下功耗极低，但无法访问Flash
 *         进入掉电模式后需要等待tPD(约3us)
 * @param  无
 * @retval 无
 */
void EN25QXX_PowerDown(void)
{
	uint8_t cmd = EN25X_PowerDown;
	
	// 片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送掉电命令
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// 等待tPD
	HAL_Delay(3);
}

/**
 * @brief  唤醒Flash芯片(退出掉电模式)
 * @note   从掉电模式唤醒后需要等待tRES1(约3us)
 *         唤醒后才能进行正常读写操作
 * @param  无
 * @retval 无
 */
void EN25QXX_WAKEUP(void)
{
	uint8_t cmd = EN25X_ReleasePowerDown;
	
	// 片选拉低
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, RESET);
	
	// 发送唤醒命令 0xAB
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	
	// 片选拉高
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, SET);
	
	// 等待tRES1
	HAL_Delay(3);
}



