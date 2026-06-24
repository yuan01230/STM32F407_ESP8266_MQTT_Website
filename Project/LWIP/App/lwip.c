/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * File Name          : LWIP.c
  * Description        : This file provides initialization code for LWIP
  *                      middleWare.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "lwip.h"
#include "udp_echo.h"
#include "tcp_server.h"
#include "http_server.h"
#include "tcp_client.h"
#include "mqtt_client.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#if defined ( __CC_ARM )  /* MDK ARM Compiler */
#include "lwip/sio.h"
#endif /* MDK ARM Compiler */
#include "ethernetif.h"
#include <stdio.h>

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/* Private function prototypes -----------------------------------------------*/
static void ethernet_link_status_updated(struct netif *netif);
static void Ethernet_Link_Periodic_Handle(struct netif *netif);
/* ETH Variables initialization ----------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
uint32_t EthernetLinkTimer;

/* Variables Initialization */
/* gnetif 是 LwIP 的全局网卡对象，后续收包、发包、链路状态都围绕它工作。 */
struct netif gnetif;
/* 静态 IP、子网掩码和网关，MX_LWIP_Init() 中会把数组值转换成 ip4_addr_t。 */
ip4_addr_t ipaddr;
ip4_addr_t netmask;
ip4_addr_t gw;
uint8_t IP_ADDRESS[4];
uint8_t NETMASK_ADDRESS[4];
uint8_t GATEWAY_ADDRESS[4];

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/**
  * @brief  初始化 LwIP 协议栈和以太网网卡。
  * @note
  * - 本工程使用 NO_SYS=1 裸机模式，不创建 TCP/IP 线程。
  * - 初始化完成后必须在主循环中持续调用 MX_LWIP_Process()。
  */
void MX_LWIP_Init(void)
{
  /* IP addresses initialization */
  /* 当前关闭 DHCP，设备固定使用独立的有线调试网段 192.168.10.123。 */
  /* IP_ADDRESS 是本机静态 IPv4 地址，PC 端 ping/UDP 测试时访问这个地址。 */
  IP_ADDRESS[0] = 192;
  IP_ADDRESS[1] = 168;
  IP_ADDRESS[2] = 10;
  IP_ADDRESS[3] = 123;
  /* NETMASK_ADDRESS 是子网掩码，下面 USER CODE 区会把最后一段修正为 0。 */
  NETMASK_ADDRESS[0] = 255;
  NETMASK_ADDRESS[1] = 255;
  NETMASK_ADDRESS[2] = 255;
  NETMASK_ADDRESS[3] = 1;
  /* GATEWAY_ADDRESS 是默认网关；本地直连测试时不一定实际使用。 */
  GATEWAY_ADDRESS[0] = 192;
  GATEWAY_ADDRESS[1] = 168;
  GATEWAY_ADDRESS[2] = 10;
  GATEWAY_ADDRESS[3] = 1;

/* USER CODE BEGIN IP_ADDRESSES */
  /* CubeMX 生成值曾为 255.255.255.1，这里修正为常用的 255.255.255.0。 */
  NETMASK_ADDRESS[3] = 0;
/* USER CODE END IP_ADDRESSES */

  /* Initialize the LwIP stack without RTOS */
  /* 初始化 LwIP 协议栈内部内存池、定时器和协议模块；当前工程是裸机 NO_SYS=1。 */
  lwip_init();

  /* IP addresses initialization without DHCP (IPv4) */
  /* 将 4 个字节数组转换成 LwIP 使用的 IPv4 地址结构。 */
  IP4_ADDR(&ipaddr, IP_ADDRESS[0], IP_ADDRESS[1], IP_ADDRESS[2], IP_ADDRESS[3]);
  IP4_ADDR(&netmask, NETMASK_ADDRESS[0], NETMASK_ADDRESS[1] , NETMASK_ADDRESS[2], NETMASK_ADDRESS[3]);
  IP4_ADDR(&gw, GATEWAY_ADDRESS[0], GATEWAY_ADDRESS[1], GATEWAY_ADDRESS[2], GATEWAY_ADDRESS[3]);

  /* add the network interface (IPv4/IPv6) without RTOS */
  /* 注册以太网网卡：ethernetif_init 负责底层 MAC/PHY 初始化，ethernet_input 负责以太网帧分发。 */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);

  /* Registers the default network interface */
  /* 设置为默认网卡，之后 IP 层发包默认从这块网卡出去。 */
  netif_set_default(&gnetif);

  /* We must always bring the network interface up connection or not... */
  /* 先把网卡管理状态置为 up，真实网线连接状态由 PHY 链路检测再更新。 */
  netif_set_up(&gnetif);

  /* Set the link callback function, this function is called on change of link status*/
  /* 链路 up/down 变化时调用，可在 USER CODE 里补充 LCD、串口或 LED 提示。 */
  netif_set_link_callback(&gnetif, ethernet_link_status_updated);

  /* 串口打印当前网络配置，方便上电后确认 IP 是否符合预期。 */
  printf("[ETH] LwIP init\r\n");
  printf("[ETH] IP=%u.%u.%u.%u NETMASK=%u.%u.%u.%u GW=%u.%u.%u.%u\r\n",
         (unsigned int)IP_ADDRESS[0], (unsigned int)IP_ADDRESS[1],
         (unsigned int)IP_ADDRESS[2], (unsigned int)IP_ADDRESS[3],
         (unsigned int)NETMASK_ADDRESS[0], (unsigned int)NETMASK_ADDRESS[1],
         (unsigned int)NETMASK_ADDRESS[2], (unsigned int)NETMASK_ADDRESS[3],
         (unsigned int)GATEWAY_ADDRESS[0], (unsigned int)GATEWAY_ADDRESS[1],
         (unsigned int)GATEWAY_ADDRESS[2], (unsigned int)GATEWAY_ADDRESS[3]);
  /* 启动 UDP 命令/回显服务，供 PC 端验证 UDP 收发链路。 */
  UDP_Echo_Init();
  TCP_Server_Init();
  HTTP_Server_Init();
  /* TCP_Client_Init(); */ /* 普通 TCP Client 实验已完成，当前保留用于对比。 */
  MQTT_Client_Init();

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */
}

#ifdef USE_OBSOLETE_USER_CODE_SECTION_4
/* Kept to help code migration. (See new 4_1, 4_2... sections) */
/* Avoid to use this user section which will become obsolete. */
/* USER CODE BEGIN 4 */
/* USER CODE END 4 */
#endif

/**
  * @brief  周期检查以太网 PHY 链路状态。
  * @param  netif LwIP 网卡对象，一般传入全局 gnetif。
  * @retval None
  * @note   裸机模式没有后台网络线程，所以需要主循环定时轮询网线插拔状态。
  */
static void Ethernet_Link_Periodic_Handle(struct netif *netif)
{
/* USER CODE BEGIN 4_4_1 */
/* USER CODE END 4_4_1 */

  /* Ethernet Link every 100ms */
  /* 裸机模式下没有网络线程，需要主循环定期检查 PHY 链路状态。 */
  /* 通过 HAL_GetTick() 做非阻塞计时，避免在主循环里 delay。 */
  if (HAL_GetTick() - EthernetLinkTimer >= 100)
  {
    /* 记录本次检查时间，下一次至少 100ms 后再检查。 */
    EthernetLinkTimer = HAL_GetTick();
    /* 读取 PHY 状态并同步更新 HAL ETH 和 LwIP netif。 */
    ethernet_link_check_state(netif);
  }
/* USER CODE BEGIN 4_4 */
/* USER CODE END 4_4 */
}

/**
 * ----------------------------------------------------------------------
 * Function given to help user to continue LwIP Initialization
 * Up to user to complete or change this function ...
 * Up to user to call this function in main.c in while (1) of main(void)
 *-----------------------------------------------------------------------
 * Read a received packet from the Ethernet buffers
 * Send it to the lwIP stack for handling
 * Handle timeouts if LWIP_TIMERS is set and without RTOS
 * Handle the llink status if LWIP_NETIF_LINK_CALLBACK is set and without RTOS
 */
/**
  * @brief  LwIP 裸机轮询入口。
  * @note
  * - 该函数应放在 main() 的 while(1) 中反复调用。
  * - 负责收包、协议定时器、链路状态检查和 MQTT 状态机推进。
  */
void MX_LWIP_Process(void)
{
/* USER CODE BEGIN 4_1 */
/* USER CODE END 4_1 */
  /* 从 ETH DMA 描述符中取出收到的数据包，并交给 LwIP 协议栈处理。 */
  ethernetif_input(&gnetif);

/* USER CODE BEGIN 4_2 */
/* USER CODE END 4_2 */
  /* Handle timeouts */
  /* 处理 ARP、TCP 等协议定时器；NO_SYS=1 时必须在主循环持续调用。 */
  sys_check_timeouts();

  /* 轮询 PHY 链路状态，网线插拔或速率变化会在这里同步到 netif。 */
  Ethernet_Link_Periodic_Handle(&gnetif);
  /* TCP_Client_Process(); */
  MQTT_Client_Process();

/* USER CODE BEGIN 4_3 */
/* USER CODE END 4_3 */
}

/**
  * @brief  Notify the User about the network interface config status
  * @param  netif: the network interface
  * @retval None
  */
static void ethernet_link_status_updated(struct netif *netif)
{
  /* netif_is_up 表示网卡管理状态为 up；链路状态由 ethernet_link_check_state 同步。 */
  if (netif_is_up(netif))
  {
    /* 链路可用时可在这里扩展 LCD 提示、LED 指示或重启应用层服务。 */
/* USER CODE BEGIN 5 */
/* USER CODE END 5 */
  }
  else /* netif is down */
  {
    /* 链路断开时可在这里扩展告警提示或清理应用层连接状态。 */
/* USER CODE BEGIN 6 */
/* USER CODE END 6 */
  }
}

#if defined ( __CC_ARM )  /* MDK ARM Compiler */
/**
  * @brief  MDK 编译器环境下的 LwIP 串口打开占位函数。
  * @param  devnum 串口设备编号，当前工程未使用。
  * @retval 串口句柄；当前为 dummy code，固定返回 0。
  * @note   仅在 __CC_ARM 条件下编译，GCC/CLion 构建不会使用这些 sio_* 函数。
  */
sio_fd_t sio_open(u8_t devnum)
{
  /* sd 是 LwIP sio 接口要求返回的串口句柄。 */
  sio_fd_t sd;

/* USER CODE BEGIN 7 */
  /* 当前工程没有通过 LwIP sio 使用串口，因此返回占位句柄。 */
  sd = 0; // dummy code
/* USER CODE END 7 */

  return sd;
}

/**
  * @brief  MDK 环境下的 LwIP 串口发送占位函数。
  * @param  c  要发送的字符。
  * @param  fd sio_open 返回的串口句柄。
  * @note   当前工程未使用 LwIP sio 串口通道，因此函数体为空。
  */
void sio_send(u8_t c, sio_fd_t fd)
{
/* USER CODE BEGIN 8 */
  /* 未接入实际串口发送逻辑；如启用 PPP/串口网络，可在这里调用 HAL_UART_Transmit。 */
/* USER CODE END 8 */
}

/**
  * @brief  MDK 环境下的 LwIP 串口阻塞读取占位函数。
  * @param  fd   串口句柄。
  * @param  data 接收缓冲区。
  * @param  len  最多读取字节数。
  * @retval 实际读取字节数；当前固定返回 0。
  * @note   当前工程不使用 sio，因此没有真正阻塞等待串口数据。
  */
u32_t sio_read(sio_fd_t fd, u8_t *data, u32_t len)
{
  /* recved_bytes 保存实际接收字节数。 */
  u32_t recved_bytes;

/* USER CODE BEGIN 9 */
  /* 没有接入串口读取逻辑，返回 0 表示未收到数据。 */
  recved_bytes = 0; // dummy code
/* USER CODE END 9 */
  return recved_bytes;
}

/**
  * @brief  MDK 环境下的 LwIP 串口非阻塞读取占位函数。
  * @param  fd   串口句柄。
  * @param  data 接收缓冲区。
  * @param  len  最多读取字节数。
  * @retval 实际读取字节数；当前固定返回 0。
  */
u32_t sio_tryread(sio_fd_t fd, u8_t *data, u32_t len)
{
  /* recved_bytes 保存本次非阻塞读取到的字节数。 */
  u32_t recved_bytes;

/* USER CODE BEGIN 10 */
  /* 没有接入串口读取逻辑，返回 0 表示当前无数据。 */
  recved_bytes = 0; // dummy code
/* USER CODE END 10 */
  return recved_bytes;
}
#endif /* MDK ARM Compiler */
