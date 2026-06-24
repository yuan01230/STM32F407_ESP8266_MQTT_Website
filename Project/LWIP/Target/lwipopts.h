/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : Target/lwipopts.h
  * Description        : This file overrides LwIP stack default configuration
  *                      done in opt.h file.
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

/* Define to prevent recursive inclusion --------------------------------------*/
#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#include "main.h"

/*-----------------------------------------------------------------------------*/
/* Current version of LwIP supported by CubeMx: 2.1.2 -*/
/*-----------------------------------------------------------------------------*/

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

#ifdef __cplusplus
 extern "C" {
#endif

/* STM32CubeMX Specific Parameters (not defined in opt.h) ---------------------*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- WITH_RTOS disabled (Since FREERTOS is not set) -----*/
/* 当前工程没有使用 FreeRTOS，LwIP 运行在裸机主循环模式。 */
#define WITH_RTOS 0
/*----- CHECKSUM_BY_HARDWARE enabled -----*/
/* 启用 STM32 ETH MAC 硬件校验和，减轻 CPU 对 IP/UDP/TCP 校验和的计算负担。 */
#define CHECKSUM_BY_HARDWARE 1
/*-----------------------------------------------------------------------------*/

/* LwIP Stack Parameters (modified compared to initialization value in opt.h) -*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- Value in opt.h for NO_SYS: 0 -----*/
/* NO_SYS=1 表示不使用 LwIP 系统线程和邮箱，应用必须循环调用 MX_LWIP_Process()。 */
#define NO_SYS 1
/*----- Value in opt.h for SYS_LIGHTWEIGHT_PROT: 1 -----*/
/* 裸机模式下关闭轻量级临界区保护；如果以后引入 RTOS，需要重新评估。 */
#define SYS_LIGHTWEIGHT_PROT 0
/*----- Value in opt.h for MEM_ALIGNMENT: 1 -----*/
/* STM32F4 以 4 字节对齐访问效率更高，也符合 DMA 缓冲常用对齐要求。 */
#define MEM_ALIGNMENT 4
/*----- Value in opt.h for LWIP_ETHERNET: LWIP_ARP || PPPOE_SUPPORT -*/
/* 启用以太网链路层支持，ARP 和 netif 以太网输出依赖该配置。 */
#define LWIP_ETHERNET 1
/*----- Value in opt.h for LWIP_DNS_SECURE: (LWIP_DNS_SECURE_RAND_XID | LWIP_DNS_SECURE_NO_MULTIPLE_OUTSTANDING | LWIP_DNS_SECURE_RAND_SRC_PORT) -*/
#define LWIP_DNS_SECURE 7
/*----- Value in opt.h for TCP_SND_QUEUELEN: (4*TCP_SND_BUF + (TCP_MSS - 1))/TCP_MSS -----*/
#define TCP_SND_QUEUELEN 9
/*----- Value in opt.h for TCP_SNDLOWAT: LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/2), (2 * TCP_MSS) + 1), (TCP_SND_BUF) - 1) -*/
#define TCP_SNDLOWAT 1071
/*----- Value in opt.h for TCP_SNDQUEUELOWAT: LWIP_MAX(TCP_SND_QUEUELEN)/2, 5) -*/
#define TCP_SNDQUEUELOWAT 5
/*----- Value in opt.h for TCP_WND_UPDATE_THRESHOLD: LWIP_MIN(TCP_WND/4, TCP_MSS*4) -----*/
#define TCP_WND_UPDATE_THRESHOLD 536
/*----- Value in opt.h for LWIP_NETIF_LINK_CALLBACK: 0 -----*/
/* 开启链路状态回调，网线插拔时 ethernet_link_status_updated() 会被触发。 */
#define LWIP_NETIF_LINK_CALLBACK 1
/*----- Value in opt.h for LWIP_NETCONN: 1 -----*/
/* 关闭 netconn API。本工程使用 raw API，代码更轻量，但需要自己处理回调。 */
#define LWIP_NETCONN 0
/*----- Value in opt.h for LWIP_SOCKET: 1 -----*/
/* 关闭 BSD socket API，避免引入 netconn 和额外内存开销。 */
#define LWIP_SOCKET 0
/*----- Value in opt.h for RECV_BUFSIZE_DEFAULT: INT_MAX -----*/
#define RECV_BUFSIZE_DEFAULT 2000000000
/*----- Value in opt.h for LWIP_STATS: 1 -----*/
/* 关闭 LwIP 统计功能，节省 RAM/代码空间；调试内存池问题时可临时打开。 */
#define LWIP_STATS 0
/*----- Value in opt.h for CHECKSUM_GEN_IP: 1 -----*/
/* 下列校验和生成/检查交给 ETH 硬件完成，因此软件侧关闭。 */
#define CHECKSUM_GEN_IP 0
/*----- Value in opt.h for CHECKSUM_GEN_UDP: 1 -----*/
#define CHECKSUM_GEN_UDP 0
/*----- Value in opt.h for CHECKSUM_GEN_TCP: 1 -----*/
#define CHECKSUM_GEN_TCP 0
/*----- Value in opt.h for CHECKSUM_GEN_ICMP: 1 -----*/
#define CHECKSUM_GEN_ICMP 0
/*----- Value in opt.h for CHECKSUM_GEN_ICMP6: 1 -----*/
#define CHECKSUM_GEN_ICMP6 0
/*----- Value in opt.h for CHECKSUM_CHECK_IP: 1 -----*/
#define CHECKSUM_CHECK_IP 0
/*----- Value in opt.h for CHECKSUM_CHECK_UDP: 1 -----*/
#define CHECKSUM_CHECK_UDP 0
/*----- Value in opt.h for CHECKSUM_CHECK_TCP: 1 -----*/
#define CHECKSUM_CHECK_TCP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP: 1 -----*/
#define CHECKSUM_CHECK_ICMP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP6: 1 -----*/
#define CHECKSUM_CHECK_ICMP6 0
/*-----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /*__LWIPOPTS__H__ */
