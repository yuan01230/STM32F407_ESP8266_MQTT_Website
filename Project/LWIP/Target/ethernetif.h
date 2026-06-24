/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * File Name          : ethernetif.h
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

#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__
#include "lwip/err.h"
#include "lwip/netif.h"

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Exported functions ------------------------------------------------------- */
/* 注册给 netif_add() 的初始化函数，负责建立 LwIP netif 与 ETH HAL 的连接。 */
err_t ethernetif_init(struct netif *netif);

/* 裸机主循环调用：从 ETH DMA 读取收到的帧并交给 LwIP 协议栈。 */
void ethernetif_input(struct netif *netif);
/* 周期检查 PHY 链路状态，并同步更新 MAC 速率/双工和 netif up/down 状态。 */
void ethernet_link_check_state(struct netif *netif);

void Error_Handler(void);
/* LwIP 时间基接口，NO_SYS=1 时协议定时器依赖这些函数。 */
u32_t sys_jiffies(void);
u32_t sys_now(void);

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
#endif
