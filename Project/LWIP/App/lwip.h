/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : LWIP.h
  * Description        : This file provides code for the configuration
  *                      of the LWIP.
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
  *************************************************************************

  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __mx_lwip_H
#define __mx_lwip_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "ethernetif.h"

/* Includes for RTOS ---------------------------------------------------------*/
#if WITH_RTOS
#include "lwip/tcpip.h"
#endif /* WITH_RTOS */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Global Variables ----------------------------------------------------------*/
/* ETH HAL 句柄在 ethernetif.c 中定义，LwIP 底层网卡驱动会使用它访问 MAC/DMA。 */
extern ETH_HandleTypeDef heth;

/* LWIP init function */
/* 初始化 LwIP 协议栈、网卡对象、静态 IP，并启动 UDP echo 测试服务。 */
void MX_LWIP_Init(void);

#if !WITH_RTOS
/* USER CODE BEGIN 1 */
/* Function defined in lwip.c to:
 *   - Read a received packet from the Ethernet buffers
 *   - Send it to the lwIP stack for handling
 *   - Handle timeouts if NO_SYS_NO_TIMERS not set
 */
/* 裸机模式下需要在 main 的 while(1) 中持续调用，负责收包和协议定时器。 */
void MX_LWIP_Process(void);

/* USER CODE END 1 */
#endif /* WITH_RTOS */

#ifdef __cplusplus
}
#endif
#endif /*__ mx_lwip_H */

/**
  * @}
  */

/**
  * @}
  */
