/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : ethernetif.c
  * Description        : This file provides code for the configuration
  *                      of the ethernetif.c MiddleWare.
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
#include "main.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ieee.h"
#include "lwip/ethip6.h"
#include "ethernetif.h"
#include "lan8742.h"
#include <string.h>
#include <stdio.h>

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Private define ------------------------------------------------------------*/

/* Network interface name */
/* LwIP 网卡短名称，调试时可看到该接口名为 "st"。 */
#define IFNAME0 's'
#define IFNAME1 't'

/* ETH Setting  */
/* HAL_ETH_Transmit 的发送超时时间，单位由 HAL 内部按 tick 处理。 */
#define ETH_DMA_TRANSMIT_TIMEOUT               ( 20U )
/* 发送 pbuf 链时最多允许使用的 ETH buffer 数量。 */
#define ETH_TX_BUFFER_MAX             ((ETH_TX_DESC_CNT) * 2U)
#define ETH_PHY_MAX_DEV_ADDR          (31U)
#define ETH_DEBUG_VERBOSE             (0U)
#define ETH_DEBUG_ARP                 (0U)
#define ETH_DEBUG_ICMP                (1U)
#define ETH_ARP_ANNOUNCE_COUNT        (0U)
#define ETH_ARP_ANNOUNCE_DELAY_MS     (300U)
#define ETH_ARP_ANNOUNCE_INTERVAL_MS  (1000U)
#define ETH_ARP_PEER_PROBE_ENABLED    (0U)
#define ETH_ARP_PEER_IP0              (192U)
#define ETH_ARP_PEER_IP1              (168U)
#define ETH_ARP_PEER_IP2              (10U)
#define ETH_ARP_PEER_IP3              (100U)

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/* Private variables ---------------------------------------------------------*/
/*
@Note: This interface is implemented to operate in zero-copy mode only:
        - Rx Buffers will be allocated from LwIP stack Rx memory pool,
          then passed to ETH HAL driver.
        - Tx Buffers will be allocated from LwIP stack memory heap,
          then passed to ETH HAL driver.

@Notes:
  1.a. ETH DMA Rx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_RX_DESC_CNT in ETH GUI (Rx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h
  1.b. ETH DMA Tx descriptors must be contiguous, the default count is 4,
       to customize it please redefine ETH_TX_DESC_CNT in ETH GUI (Tx Descriptor Length)
       so that updated value will be generated in stm32xxxx_hal_conf.h

  2.a. Rx Buffers number must be between ETH_RX_DESC_CNT and 2*ETH_RX_DESC_CNT
  2.b. Rx Buffers must have the same size: ETH_RX_BUF_SIZE, this value must
       passed to ETH DMA in the init field (heth.Init.RxBuffLen)
  2.c  The RX Ruffers addresses and sizes must be properly defined to be aligned
       to L1-CACHE line size (32 bytes).
*/

/* Data Type Definitions */
typedef enum
{
  RX_ALLOC_OK       = 0x00,
  RX_ALLOC_ERROR    = 0x01
} RxAllocStatusTypeDef;

typedef struct
{
  struct pbuf_custom pbuf_custom;
  /* 接收缓冲区按 32 字节对齐，满足 ETH DMA/cache 对齐要求。 */
  uint8_t buff[(ETH_RX_BUF_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;

/* Memory Pool Declaration */
/* 给 ETH 接收路径预留的 pbuf 池，HAL 回调从这里分配收包缓冲。 */
#define ETH_RX_BUFFER_CNT             12U
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

/* Variable Definitions */
static uint8_t RxAllocStatus;
static uint32_t EthRxPacketCount;
static uint32_t EthTxPacketCount;
static uint32_t EthRxArpCount;
static uint32_t EthTxArpCount;
static uint32_t EthRxIpCount;
static uint32_t EthTxIpCount;
static uint32_t EthLastDmaSr;
static uint32_t EthLastDmaOmr;
static uint32_t EthLastMacCr;
static uint8_t EthArpAnnounceRemaining;
static uint32_t EthArpAnnounceNextTick;
static uint32_t EthArpAnnounceFailCount;

ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Global Ethernet handle */
ETH_HandleTypeDef heth;
ETH_TxPacketConfig TxConfig;

/* Private function prototypes -----------------------------------------------*/
int32_t ETH_PHY_IO_Init(void);
int32_t ETH_PHY_IO_DeInit (void);
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
int32_t ETH_PHY_IO_GetTick(void);

lan8742_Object_t LAN8742;
/* PHY 驱动通过这组回调访问 MDIO/MDC，总线读写最终落到 HAL_ETH_Read/WritePHYRegister。 */
lan8742_IOCtx_t  LAN8742_IOCtx = {ETH_PHY_IO_Init,
                                  ETH_PHY_IO_DeInit,
                                  ETH_PHY_IO_WriteReg,
                                  ETH_PHY_IO_ReadReg,
                                  ETH_PHY_IO_GetTick};

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */

/* Private functions ---------------------------------------------------------*/
void pbuf_free_custom(struct pbuf *p);
#if (ETH_DEBUG_VERBOSE != 0U)
static void ETH_PrintPhyDebug(void);
#endif
#if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_DEBUG_ARP != 0U) || (ETH_DEBUG_ICMP != 0U))
static void ETH_PrintPacketDebug(const char *dir, struct pbuf *p);
#endif
static err_t low_level_output(struct netif *netif, struct pbuf *p);
static uint8_t ETH_IsIcmpPacket(struct pbuf *p);
static err_t ETH_SendGratuitousArpReply(struct netif *netif);
static void ETH_StartArpAnnounce(void);
static void ETH_StopArpAnnounce(void);
static void ETH_ProcessArpAnnounce(struct netif *netif);
#if (ETH_DEBUG_VERBOSE != 0U)
static const char *ETH_DmaTxStateText(uint32_t dmasr);
static const char *ETH_DmaRxStateText(uint32_t dmasr);
static void ETH_PrintDmaStatus(uint32_t dmasr);
static void ETH_PrintDescDebug(void);
#endif

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

static uint8_t ETH_IsIcmpPacket(struct pbuf *p)
{
  uint8_t *payload = NULL;

  if ((p == NULL) || (p->payload == NULL) || (p->len < 34U))
  {
    return 0U;
  }

  payload = (uint8_t *)p->payload;
  return ((payload[12] == 0x08U) && (payload[13] == 0x00U) && (payload[23] == 1U)) ? 1U : 0U;
}

static err_t ETH_SendGratuitousArpReply(struct netif *netif)
{
  struct pbuf *p;
  uint8_t *payload;
  const ip4_addr_t *ipaddr;
  err_t err;

  if ((netif == NULL) || (netif->hwaddr_len != ETH_HWADDR_LEN) || !netif_is_link_up(netif))
  {
    return ERR_ARG;
  }

  p = pbuf_alloc(PBUF_RAW, 42U, PBUF_RAM);
  if (p == NULL)
  {
    return ERR_MEM;
  }

  payload = (uint8_t *)p->payload;
  ipaddr = netif_ip4_addr(netif);

  (void)memset(payload, 0, 42U);

  /* Ethernet header: broadcast destination, local source, EtherType ARP. */
  (void)memset(&payload[0], 0xFF, 6U);
  (void)memcpy(&payload[6], netif->hwaddr, 6U);
  payload[12] = 0x08U;
  payload[13] = 0x06U;

  /* ARP payload: Ethernet/IPv4, reply, our MAC/IP is-at our MAC/IP. */
  payload[14] = 0x00U;
  payload[15] = 0x01U;
  payload[16] = 0x08U;
  payload[17] = 0x00U;
  payload[18] = 0x06U;
  payload[19] = 0x04U;
  payload[20] = 0x00U;
  payload[21] = 0x02U;

  (void)memcpy(&payload[22], netif->hwaddr, 6U);
  payload[28] = ip4_addr1(ipaddr);
  payload[29] = ip4_addr2(ipaddr);
  payload[30] = ip4_addr3(ipaddr);
  payload[31] = ip4_addr4(ipaddr);
  (void)memset(&payload[32], 0xFF, 6U);
  payload[38] = ip4_addr1(ipaddr);
  payload[39] = ip4_addr2(ipaddr);
  payload[40] = ip4_addr3(ipaddr);
  payload[41] = ip4_addr4(ipaddr);

  err = low_level_output(netif, p);
  pbuf_free(p);
  return err;
}

static void ETH_StartArpAnnounce(void)
{
  EthArpAnnounceRemaining = ETH_ARP_ANNOUNCE_COUNT;
  EthArpAnnounceNextTick = HAL_GetTick() + ETH_ARP_ANNOUNCE_DELAY_MS;
  EthArpAnnounceFailCount = 0U;
}

static void ETH_StopArpAnnounce(void)
{
  EthArpAnnounceRemaining = 0U;
}

static void ETH_ProcessArpAnnounce(struct netif *netif)
{
  uint32_t now;
  uint32_t announce_index;
  err_t garp_err;
  err_t garp_reply_err;
  err_t peer_err = ERR_OK;

  if ((netif == NULL) || (EthArpAnnounceRemaining == 0U) ||
      !netif_is_up(netif) || !netif_is_link_up(netif))
  {
    return;
  }

  now = HAL_GetTick();
  if ((int32_t)(now - EthArpAnnounceNextTick) < 0)
  {
    return;
  }

  announce_index = (ETH_ARP_ANNOUNCE_COUNT - EthArpAnnounceRemaining) + 1U;
  garp_err = etharp_gratuitous(netif);
  garp_reply_err = ETH_SendGratuitousArpReply(netif);

#if (ETH_ARP_PEER_PROBE_ENABLED != 0U)
  {
    ip4_addr_t peer_ip;
    IP4_ADDR(&peer_ip, ETH_ARP_PEER_IP0, ETH_ARP_PEER_IP1, ETH_ARP_PEER_IP2, ETH_ARP_PEER_IP3);
    peer_err = etharp_request(netif, &peer_ip);
  }
#endif

  if ((garp_err != ERR_OK) || (garp_reply_err != ERR_OK) || (peer_err != ERR_OK))
  {
    EthArpAnnounceFailCount++;
  }

  EthArpAnnounceRemaining--;
  EthArpAnnounceNextTick = now + ETH_ARP_ANNOUNCE_INTERVAL_MS;

  if (EthArpAnnounceRemaining == 0U)
  {
    printf("[ETH] arp announce complete: %lu attempts, %lu failures\r\n",
           (unsigned long)announce_index,
           (unsigned long)EthArpAnnounceFailCount);
  }
}

#if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_DEBUG_ARP != 0U) || (ETH_DEBUG_ICMP != 0U))
/**
  * @brief  打印以太网帧的简要信息，便于确认 ARP/IP 收发是否正常。
  * @param  dir 方向文本，一般为 "RX" 或 "TX"。
  * @param  p   要分析的 LwIP pbuf，payload 需指向完整以太网帧头。
  * @note   ETH_DEBUG_ARP 默认打开，只打印 ARP；ETH_DEBUG_VERBOSE 打开后会打印更多帧。
  */
static void ETH_PrintPacketDebug(const char *dir, struct pbuf *p)
{
  /* hdr 用结构体方式解释以太网帧头，便于打印源/目的 MAC。 */
  const struct eth_hdr *hdr = NULL;
  /* payload 以字节数组方式访问，方便读取 EtherType 字段。 */
  uint8_t *payload = NULL;
  /* type 保存以太网帧类型，例如 ARP=0x0806、IPv4=0x0800。 */
  uint16_t type = 0U;

  /* pbuf 无效、payload 为空或长度不足以太网头时，不能继续解析。 */
  if ((p == NULL) || (p->payload == NULL) || (p->len < SIZEOF_ETH_HDR))
  {
    printf("[ETH] %s invalid packet p=%p\r\n", dir, (void *)p);
    return;
  }

  /* 取出原始 payload 地址，同时按以太网头结构解释。 */
  payload = (uint8_t *)p->payload;
  hdr = (const struct eth_hdr *)payload;
  /* EtherType 位于以太网头第 12、13 字节，按网络字节序组合成 16 位值。 */
  type = ((uint16_t)payload[12] << 8) | (uint16_t)payload[13];

  /* ARP 包最适合用于确认局域网发现和 ping 前置流程。 */
  if (type == ETHTYPE_ARP)
  {
    if (p->len >= 42U)
    {
      uint16_t arp_op = ((uint16_t)payload[20] << 8) | (uint16_t)payload[21];
      const char *arp_op_text = (arp_op == 1U) ? "request" : ((arp_op == 2U) ? "reply" : "other");

      printf("[ETH] %s ARP %s len=%u tot=%u src=%02X:%02X:%02X:%02X:%02X:%02X dst=%02X:%02X:%02X:%02X:%02X:%02X sender=%u.%u.%u.%u target=%u.%u.%u.%u\r\n",
             dir,
             arp_op_text,
             (unsigned int)p->len,
             (unsigned int)p->tot_len,
             hdr->src.addr[0], hdr->src.addr[1], hdr->src.addr[2],
             hdr->src.addr[3], hdr->src.addr[4], hdr->src.addr[5],
             hdr->dest.addr[0], hdr->dest.addr[1], hdr->dest.addr[2],
             hdr->dest.addr[3], hdr->dest.addr[4], hdr->dest.addr[5],
             payload[28], payload[29], payload[30], payload[31],
             payload[38], payload[39], payload[40], payload[41]);
    }
    else
    {
      printf("[ETH] %s ARP len=%u tot=%u src=%02X:%02X:%02X:%02X:%02X:%02X dst=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
             dir,
             (unsigned int)p->len,
             (unsigned int)p->tot_len,
             hdr->src.addr[0], hdr->src.addr[1], hdr->src.addr[2],
             hdr->src.addr[3], hdr->src.addr[4], hdr->src.addr[5],
             hdr->dest.addr[0], hdr->dest.addr[1], hdr->dest.addr[2],
             hdr->dest.addr[3], hdr->dest.addr[4], hdr->dest.addr[5]);
    }
  }
  else if (type == ETHTYPE_IP)
  {
    if (p->len >= 34U)
    {
      uint8_t ip_header_len = (uint8_t)((payload[14] & 0x0FU) * 4U);
      uint8_t ip_proto = payload[23];

      if ((ip_proto == 1U) && (p->len >= (u16_t)(14U + ip_header_len + 1U)))
      {
        printf("[ETH] %s IPv4 ICMP type=%u len=%u tot=%u src=%u.%u.%u.%u dst=%u.%u.%u.%u\r\n",
               dir,
               payload[14U + ip_header_len],
               (unsigned int)p->len,
               (unsigned int)p->tot_len,
               payload[26], payload[27], payload[28], payload[29],
               payload[30], payload[31], payload[32], payload[33]);
      }
      else
      {
        printf("[ETH] %s IPv4 proto=%u len=%u tot=%u src=%u.%u.%u.%u dst=%u.%u.%u.%u\r\n",
               dir,
               ip_proto,
               (unsigned int)p->len,
               (unsigned int)p->tot_len,
               payload[26], payload[27], payload[28], payload[29],
               payload[30], payload[31], payload[32], payload[33]);
      }
    }
    else
    {
      printf("[ETH] %s IPv4 len=%u tot=%u\r\n",
             dir,
             (unsigned int)p->len,
             (unsigned int)p->tot_len);
    }
  }
  else
  {
    /* 其他类型保留 EtherType，后续可据此判断是否为 IPv6、LLDP 等帧。 */
    printf("[ETH] %s type=0x%04X len=%u tot=%u\r\n",
           dir,
           (unsigned int)type,
           (unsigned int)p->len,
           (unsigned int)p->tot_len);
  }
}
#endif

#if (ETH_DEBUG_VERBOSE != 0U)
/* 将 DMA 发送状态位转换成可读文本，方便串口日志定位 DMA 卡住的位置。 */
static const char *ETH_DmaTxStateText(uint32_t dmasr)
{
  /* 只取发送状态位 TPS，其它标志位由 ETH_PrintDmaStatus 单独打印。 */
  switch (dmasr & ETH_DMASR_TPS)
  {
  case ETH_DMASR_TPS_Stopped:
    return "stopped";
  case ETH_DMASR_TPS_Fetching:
    return "fetching";
  case ETH_DMASR_TPS_Waiting:
    return "waiting";
  case ETH_DMASR_TPS_Reading:
    return "reading";
  case ETH_DMASR_TPS_Suspended:
    return "suspended";
  case ETH_DMASR_TPS_Closing:
    return "closing";
  default:
    return "unknown";
  }
}
#endif

#if (ETH_DEBUG_VERBOSE != 0U)
/* 将 DMA 接收状态位转换成可读文本，配合 DMASR 寄存器调试收包问题。 */
static const char *ETH_DmaRxStateText(uint32_t dmasr)
{
  /* 只取接收状态位 RPS，用于判断 DMA 接收流程停在哪个阶段。 */
  switch (dmasr & ETH_DMASR_RPS)
  {
  case ETH_DMASR_RPS_Stopped:
    return "stopped";
  case ETH_DMASR_RPS_Fetching:
    return "fetching";
  case ETH_DMASR_RPS_Waiting:
    return "waiting";
  case ETH_DMASR_RPS_Suspended:
    return "suspended";
  case ETH_DMASR_RPS_Closing:
    return "closing";
  case ETH_DMASR_RPS_Queuing:
    return "queuing";
  default:
    return "unknown";
  }
}
#endif

#if (ETH_DEBUG_VERBOSE != 0U)
/* 打印 DMA 状态寄存器中的关键标志位。 */
static void ETH_PrintDmaStatus(uint32_t dmasr)
{
  /* 将常见 DMA 状态位拼成一行日志，便于串口上快速看出异常标志。 */
  printf("[ETH] dma tx=%s rx=%s flags:%s%s%s%s%s%s%s%s%s\r\n",
         ETH_DmaTxStateText(dmasr),
         ETH_DmaRxStateText(dmasr),
         ((dmasr & ETH_DMASR_TS) != 0U) ? " TS" : "",
         ((dmasr & ETH_DMASR_TBUS) != 0U) ? " TBUS" : "",
         ((dmasr & ETH_DMASR_TUS) != 0U) ? " TUS" : "",
         ((dmasr & ETH_DMASR_RS) != 0U) ? " RS" : "",
         ((dmasr & ETH_DMASR_RBUS) != 0U) ? " RBUS" : "",
         ((dmasr & ETH_DMASR_ETS) != 0U) ? " ETS" : "",
         ((dmasr & ETH_DMASR_NIS) != 0U) ? " NIS" : "",
         ((dmasr & ETH_DMASR_AIS) != 0U) ? " AIS" : "",
         ((dmasr & ETH_DMASR_FBES) != 0U) ? " FBES" : "");
}
#endif

#if (ETH_DEBUG_VERBOSE != 0U)
/* 打印当前 Tx/Rx DMA 描述符状态，用于排查描述符归还或 DMA 挂起问题。 */
static void ETH_PrintDescDebug(void)
{
  /* tx_desc/rx_desc 指向当前 HAL 认为正在处理的 DMA 描述符。 */
  ETH_DMADescTypeDef *tx_desc = NULL;
  ETH_DMADescTypeDef *rx_desc = NULL;

  /* CurTxDesc 有效时取当前发送描述符地址。 */
  if (heth.TxDescList.CurTxDesc < ETH_TX_DESC_CNT)
  {
    tx_desc = (ETH_DMADescTypeDef *)heth.TxDescList.TxDesc[heth.TxDescList.CurTxDesc];
  }
  /* RxDescIdx 有效时取当前接收描述符地址。 */
  if (heth.RxDescList.RxDescIdx < ETH_RX_DESC_CNT)
  {
    rx_desc = (ETH_DMADescTypeDef *)heth.RxDescList.RxDesc[heth.RxDescList.RxDescIdx];
  }

  /* 打印发送描述符寄存器内容，可用于判断 OWN 位、buffer 地址和长度是否正常。 */
  if (tx_desc != NULL)
  {
    printf("[ETH] txdesc idx=%lu use=%lu rel=%lu D0=0x%08lX D1=0x%08lX D2=0x%08lX D3=0x%08lX\r\n",
           (unsigned long)heth.TxDescList.CurTxDesc,
           (unsigned long)heth.TxDescList.BuffersInUse,
           (unsigned long)heth.TxDescList.releaseIndex,
           (unsigned long)tx_desc->DESC0,
           (unsigned long)tx_desc->DESC1,
           (unsigned long)tx_desc->DESC2,
           (unsigned long)tx_desc->DESC3);
  }

  /* 打印接收描述符寄存器内容，可用于判断 DMA 是否持续占用或缓冲是否断链。 */
  if (rx_desc != NULL)
  {
    printf("[ETH] rxdesc idx=%lu build_idx=%lu build_cnt=%lu D0=0x%08lX D1=0x%08lX D2=0x%08lX D3=0x%08lX\r\n",
           (unsigned long)heth.RxDescList.RxDescIdx,
           (unsigned long)heth.RxDescList.RxBuildDescIdx,
           (unsigned long)heth.RxDescList.RxBuildDescCnt,
           (unsigned long)rx_desc->DESC0,
           (unsigned long)rx_desc->DESC1,
           (unsigned long)rx_desc->DESC2,
           (unsigned long)rx_desc->DESC3);
  }
}
#endif

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
/**
 * @brief  初始化以太网底层硬件，并把硬件能力同步到 LwIP netif。
 * @param  netif 已由 LwIP 创建的网卡对象。
 * @note   调用链为 MX_LWIP_Init() -> netif_add() -> ethernetif_init() -> low_level_init()。
 *         这里完成 ETH MAC、DMA 描述符、RX 内存池和 PHY 驱动初始化。
 */
static void low_level_init(struct netif *netif)
{
  /* 保存 HAL_ETH_Init 的结果，后面决定是否继续检查 PHY 链路。 */
  HAL_StatusTypeDef hal_eth_init_status = HAL_OK;
  /* Start ETH HAL Init */

  /* MACAddr 是本地临时数组，HAL_ETH_Init 会复制/使用它配置 MAC 地址。 */
   uint8_t MACAddr[6] ;
  /* 绑定 ETH 外设实例。 */
  heth.Instance = ETH;
  /* 本机 MAC 地址。若同一局域网有多块板子，需要保证每块板不同。 */
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  /* 当前硬件按 RMII 连接 LAN8720/LAN8742 类 PHY。 */
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  /* 单个 RX DMA buffer 长度，需能容纳标准以太网帧。 */
  heth.Init.RxBuffLen = 1536;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  /* 初始化 ETH MAC/DMA 和底层 GPIO/中断，HAL 会回调 HAL_ETH_MspInit。 */
  hal_eth_init_status = HAL_ETH_Init(&heth);

  /* 清空发送配置结构体，避免保留上一次或栈上的随机值。 */
  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  /* 发送时由硬件补充 IP/TCP/UDP 校验和，并自动追加 CRC/PAD。 */
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

  /* End ETH HAL Init */

  /* Initialize the RX POOL */
  /* 初始化零拷贝接收缓冲池，后续 HAL_ETH_RxAllocateCallback 会从这里取 pbuf。 */
  LWIP_MEMPOOL_INIT(RX_POOL);

#if LWIP_ARP || LWIP_ETHERNET

  /* set MAC hardware address length */
  /* 把 HAL 中配置的 MAC 地址同步给 LwIP netif。 */
  netif->hwaddr_len = ETH_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] =  heth.Init.MACAddr[0];
  netif->hwaddr[1] =  heth.Init.MACAddr[1];
  netif->hwaddr[2] =  heth.Init.MACAddr[2];
  netif->hwaddr[3] =  heth.Init.MACAddr[3];
  netif->hwaddr[4] =  heth.Init.MACAddr[4];
  netif->hwaddr[5] =  heth.Init.MACAddr[5];

  /* maximum transfer unit */
  /* 以太网标准 MTU，通常为 1500 字节有效载荷。 */
  netif->mtu = ETH_MAX_PAYLOAD;

  /* Accept broadcast address and ARP traffic */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  #if LWIP_ARP
    /* 启用广播和 ARP 标志，IPv4 局域网通信需要 ARP 解析 MAC。 */
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
  #else
    /* 如果关闭 ARP，仍保留广播能力。 */
    netif->flags |= NETIF_FLAG_BROADCAST;
  #endif /* LWIP_ARP */

/* USER CODE BEGIN PHY_PRE_CONFIG */

/* USER CODE END PHY_PRE_CONFIG */
  /* Set PHY IO functions */
  /* 注册 PHY 访问接口，LAN8742_Init 会通过 MDIO 扫描实际 PHY 地址。 */
  LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);

  /* Initialize the LAN8742 ETH PHY */
  /* 这里使用 LAN8742 驱动访问 LAN8720 类 PHY；初始化失败则关闭 netif。 */
  if(LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK)
  {
    printf("[ETH] PHY init failed\r\n");
    netif_set_link_down(netif);
    netif_set_down(netif);
    return;
  }

  if (hal_eth_init_status == HAL_OK)
  {
  /* Get link state */
  /* ETH 初始化成功后立即检查一次链路，决定是否启动 MAC 收发。 */
  ethernet_link_check_state(netif);
  }
  else
  {
    /* HAL 初始化失败属于硬件/时钟/GPIO 级问题，交给全局错误处理。 */
    Error_Handler();
  }
#endif /* LWIP_ARP || LWIP_ETHERNET */

/* USER CODE BEGIN LOW_LEVEL_INIT */

/* USER CODE END LOW_LEVEL_INIT */
}

/**
  * @brief  LwIP 发包出口，把 pbuf 链转换成 ETH HAL 可发送的 DMA buffer 链。
  * @param  netif LwIP 网卡对象，本函数当前不直接使用，但符合 netif->linkoutput 签名。
  * @param  p     待发送的 MAC 帧，payload 已包含以太网头，可能由多个 pbuf 串联。
  * @retval ERR_OK 发送请求提交成功；ERR_IF 表示 buffer 数量不足或 HAL 发送失败。
  * @note   这是 LwIP 到 STM32 ETH HAL 的关键桥接函数；ARP、IPv4、UDP 最终都会走到这里。
  */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  /* i 记录当前已填充的 Txbuffer 数量。 */
  uint32_t i = 0U;
  /* q 用来遍历 LwIP 的 pbuf 链。 */
  struct pbuf *q = NULL;
  /* errval 默认成功，只有 HAL 发送失败时改为 ERR_IF。 */
  err_t errval = ERR_OK;
  /* Txbuffer 是 HAL_ETH_Transmit 接收的发送 buffer 描述数组。 */
  ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT] = {0};

  /* 清空发送 buffer 数组，避免 next 指针残留。 */
  memset(Txbuffer, 0 , ETH_TX_DESC_CNT*sizeof(ETH_BufferTypeDef));

  /* LwIP 的 pbuf 可能是链表，这里转换成 HAL_ETH_Transmit 需要的 Txbuffer 链。 */
  for(q = p; q != NULL; q = q->next)
  {
    /* pbuf 链段数超过 DMA 描述符数量时无法发送，直接返回接口错误。 */
    if(i >= ETH_TX_DESC_CNT)
      return ERR_IF;

    /* 当前 Txbuffer 指向当前 pbuf 的 payload。 */
    Txbuffer[i].buffer = q->payload;
    /* len 是当前 pbuf 片段长度，不是整包长度。 */
    Txbuffer[i].len = q->len;

    /* 将上一个 Txbuffer 的 next 指向当前 Txbuffer，组成 HAL 需要的链表。 */
    if(i>0)
    {
      Txbuffer[i-1].next = &Txbuffer[i];
    }

    /* pbuf 链尾对应 Txbuffer 链尾，next 必须为 NULL。 */
    if(q->next == NULL)
    {
      Txbuffer[i].next = NULL;
    }

    /* 继续处理下一个 pbuf 片段。 */
    i++;
  }

  /* tot_len 是整个以太网帧总长度。 */
  TxConfig.Length = p->tot_len;
  /* TxBuffer 指向刚刚构造好的 HAL buffer 链。 */
  TxConfig.TxBuffer = Txbuffer;
  /* pData 保存原始 pbuf，发送完成释放时要用到。 */
  TxConfig.pData = p;
  /* 统计和调试：只在 payload 至少包含以太网头时解析类型。 */
  if ((p->payload != NULL) && (p->len >= SIZEOF_ETH_HDR))
  {
    /* payload[12:13] 是 EtherType。 */
    uint8_t *payload = (uint8_t *)p->payload;
    /* 记录总发送帧数。 */
    EthTxPacketCount++;
    /* 0x0806 是 ARP 帧。 */
    if ((payload[12] == 0x08U) && (payload[13] == 0x06U))
    {
      EthTxArpCount++;
#if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_DEBUG_ARP != 0U))
      ETH_PrintPacketDebug("TX", p);
#endif
    }
    /* 0x0800 是 IPv4 帧。 */
    else if ((payload[12] == 0x08U) && (payload[13] == 0x00U))
    {
      EthTxIpCount++;
#if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_DEBUG_ICMP != 0U))
      if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_IsIcmpPacket(p) != 0U))
      {
      ETH_PrintPacketDebug("TX", p);
      }
#endif
    }
  }

  /* 交给 ETH DMA 发送；发送完成后 HAL 会通过 TxFreeCallback 释放 pbuf。 */
  {
    /* tx_status 保存 HAL 发送结果。 */
    HAL_StatusTypeDef tx_status;
    /* dmasr 保存 DMA 状态寄存器，用于失败日志。 */
    uint32_t dmasr = heth.Instance->DMASR;

    /*
     * 阻塞发送 API 不会像 HAL_ETH_Transmit_IT() 那样保存 pData。
     * 这里手动增加一次 pbuf 引用，并在 HAL_ETH_ReleaseTxPacket() 中释放，
     * 避免 TxDescList.BuffersInUse 长时间累积导致后续发送失败。
     */
    /* 增加 pbuf 引用计数，确保发送释放前 pbuf 不被 LwIP 提前回收。 */
    pbuf_ref(p);
    /* 让 HAL 的释放流程能找到对应 pbuf。 */
    heth.TxDescList.CurrentPacketAddress = (uint32_t *)p;
    /* 阻塞提交发送，超时时间由 ETH_DMA_TRANSMIT_TIMEOUT 控制。 */
    tx_status = HAL_ETH_Transmit(&heth, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT);
    /* 释放已经完成的 Tx 描述符和挂载的 pbuf 引用。 */
    HAL_ETH_ReleaseTxPacket(&heth);

    /* 重新读取发送后的 DMA 状态，失败时打印更准确。 */
    dmasr = heth.Instance->DMASR;
    if (tx_status != HAL_OK)
    {
      printf("[ETH] tx failed status=%ld DMASR=0x%08lX gState=%lu err=0x%08lX dmaerr=0x%08lX\r\n",
             (long)tx_status,
             (unsigned long)dmasr,
             (unsigned long)heth.gState,
             (unsigned long)heth.ErrorCode,
             (unsigned long)heth.DMAErrorCode);
#if (ETH_DEBUG_VERBOSE != 0U)
      ETH_PrintDmaStatus(dmasr);
#endif
      errval = ERR_IF;
    }
  }

  /* 返回给 LwIP，决定上层是否认为本次链路层发送成功。 */
  return errval;
}

/**
  * @brief  从 ETH DMA 接收队列中取出一帧，并返回给上层 ethernetif_input()。
  * @param  netif LwIP 网卡对象，本函数当前不直接使用，但保留标准接口语义。
  * @retval 非 NULL 表示收到一个包含 MAC 头的 pbuf；NULL 表示当前没有可处理数据或内存不足。
  */
static struct pbuf * low_level_input(struct netif *netif)
{
  /* p 保存 HAL_ETH_ReadData 返回的接收 pbuf。 */
  struct pbuf *p = NULL;

  /* 只有接收缓冲池可用时才尝试从 ETH DMA 读取一帧。 */
  if(RxAllocStatus == RX_ALLOC_OK)
  {
    /* HAL_ETH_ReadData 会通过 RxLinkCallback 组装 pbuf 链并赋给 p。 */
    HAL_ETH_ReadData(&heth, (void **)&p);
  }
  /* 收到有效以太网帧后做统计和可选调试打印。 */
  if ((p != NULL) && (p->payload != NULL) && (p->len >= SIZEOF_ETH_HDR))
  {
    /* 以字节方式访问以太网头。 */
    uint8_t *payload = (uint8_t *)p->payload;
    /* 记录总接收帧数。 */
    EthRxPacketCount++;
    /* ARP 接收统计，常用于确认 PC 是否发起地址解析。 */
    if ((payload[12] == 0x08U) && (payload[13] == 0x06U))
    {
      EthRxArpCount++;
#if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_DEBUG_ARP != 0U))
      ETH_PrintPacketDebug("RX", p);
#endif
    }
    /* IPv4 接收统计，ping、UDP、TCP 都属于这一类。 */
    else if ((payload[12] == 0x08U) && (payload[13] == 0x00U))
    {
      EthRxIpCount++;
#if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_DEBUG_ICMP != 0U))
      if ((ETH_DEBUG_VERBOSE != 0U) || (ETH_IsIcmpPacket(p) != 0U))
      {
      ETH_PrintPacketDebug("RX", p);
      }
#endif
    }
  }

  /* 返回 pbuf 给 ethernetif_input；若为 NULL 表示没有更多包。 */
  return p;
}

/**
  * @brief  LwIP 收包入口，从 ETH DMA 取包并交给协议栈分发。
  * @param  netif LwIP 网卡对象。
  * @note   该函数由 MX_LWIP_Process() 在主循环中调用；一次调用会尽量取完当前所有已到达帧。
  */
void ethernetif_input(struct netif *netif)
{
  /* p 保存每次从 low_level_input 取出的一个接收包。 */
  struct pbuf *p = NULL;

  /* 一次主循环尽量把已经到达的帧都取完，避免 DMA 接收队列堆积。 */
  do
  {
    /* 尝试读取一帧；没有数据时返回 NULL。 */
    p = low_level_input( netif );
    if (p != NULL)
    {
      /* netif->input 通常是 ethernet_input，会根据 EtherType 分发到 ARP/IP 等模块。 */
      if (netif->input( p, netif) != ERR_OK )
      {
        /* 协议栈拒收时释放 pbuf，避免接收缓冲泄漏。 */
        pbuf_free(p);
      }
    }
  /* 继续读取直到 DMA 队列暂时没有更多包。 */
  } while(p!=NULL);
}

#if !LWIP_ARP
/**
  * @brief  ARP 关闭时的 IPv4 发包占位函数。
  * @param  netif  LwIP 网卡对象。
  * @param  q      待发送 pbuf。
  * @param  ipaddr 目标 IPv4 地址。
  * @retval ERR_OK 当前占位实现固定返回成功。
  * @note   本工程启用了 LWIP_ARP，因此该函数不会参与当前编译路径。
  */
static err_t low_level_output_arp_off(struct netif *netif, struct pbuf *q, const ip4_addr_t *ipaddr)
{
  /* errval 保存返回给 LwIP 的链路层发送结果。 */
  err_t errval;
  /* 当前为占位实现，不做实际发送。 */
  errval = ERR_OK;

/* USER CODE BEGIN 5 */

/* USER CODE END 5 */

  /* 返回占位结果；若未来关闭 ARP，需要在此实现目标 MAC 获取和 low_level_output 调用。 */
  return errval;

}
#endif /* LWIP_ARP */

/**
  * @brief  初始化 LwIP 网卡对象，并绑定底层以太网输入/输出函数。
  * @param  netif LwIP 在 netif_add() 中创建并传入的网卡对象。
  * @retval ERR_OK 表示网卡初始化流程完成。
  * @note   这是 LwIP 层认识 STM32 以太网硬件的入口函数。
  */
err_t ethernetif_init(struct netif *netif)
{
  /* LwIP 断言：netif 不允许为空，否则后续写字段会崩溃。 */
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  /* 如果启用主机名功能，这里给网卡设置一个默认名称。 */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  // MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  /* 设置 LwIP 网卡短名称，组合起来是 "st"。 */
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */

#if LWIP_IPV4
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
  /* IPv4 发包先走 ARP，解析目标 MAC 后再调用 linkoutput。 */
  netif->output = etharp_output;
#else
  /* The user should write its own code in low_level_output_arp_off function */
  netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */
#endif /* LWIP_ARP || LWIP_ETHERNET */
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
  /* IPv6 发包路径，当前如果启用 IPv6，会走 ethip6_output。 */
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

  /* 链路层真正发包函数，ARP 解析出 MAC 后最终会调用 low_level_output。 */
  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  /* 初始化底层 ETH MAC、DMA 描述符、PHY，并同步初始链路状态。 */
  low_level_init(netif);

  /* 返回成功给 netif_add，表示该 netif 可以加入 LwIP 网卡列表。 */
  return ERR_OK;
}

/**
  * @brief  自定义 RX pbuf 释放回调。
  * @param  p 被 LwIP 释放的接收 pbuf。
  * @retval None
  * @note   ETH 接收缓冲来自 RX_POOL，释放时必须归还到同一个内存池。
  */
void pbuf_free_custom(struct pbuf *p)
{
  /* pbuf_custom 是 RxBuff_t 的第一个成员，可以安全转换回自定义 pbuf。 */
  struct pbuf_custom* custom_pbuf = (struct pbuf_custom*)p;
  /* 释放自定义 pbuf 时归还到 RX_POOL，供后续接收继续复用。 */
  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

  /* If the Rx Buffer Pool was exhausted, signal the ethernetif_input task to
   * call HAL_ETH_GetRxDataBuffer to rebuild the Rx descriptors. */

  if (RxAllocStatus == RX_ALLOC_ERROR)
  {
    /* 之前 RX_POOL 耗尽时会置错；现在释放了一个 pbuf，可以重新尝试收包。 */
    RxAllocStatus = RX_ALLOC_OK;
  }
}

/* USER CODE BEGIN 6 */

/**
  * @brief  返回 LwIP 协议定时器使用的毫秒时间。
  * @retval 当前 HAL tick，单位 ms。
  * @note   NO_SYS=1 且 LWIP_TIMERS=1 时，sys_check_timeouts() 依赖该时间基。
  */
u32_t sys_now(void)
{
  /* LwIP 定时器使用毫秒时间基，这里直接复用 HAL SysTick。 */
  return HAL_GetTick();
}

/* USER CODE END 6 */

/**
  * @brief  初始化 ETH 外设底层硬件资源。
  * @param  ethHandle ETH HAL 句柄。
  * @retval None
  * @note   HAL_ETH_Init() 会调用该函数；这里配置 ETH 时钟、RMII GPIO 和中断。
  */

void HAL_ETH_MspInit(ETH_HandleTypeDef* ethHandle)
{
  /* GPIO_InitStruct 用于复用配置 RMII 引脚。 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* 只处理 ETH 外设实例，避免其它外设误入。 */
  if(ethHandle->Instance==ETH)
  {
  /* USER CODE BEGIN ETH_MspInit 0 */

  /* USER CODE END ETH_MspInit 0 */
    /* Enable Peripheral clock */
    /* ETH 外设和 RMII 相关 GPIO 端口时钟。 */
    /* 先开 ETH 外设时钟，再配置对应 GPIO 端口时钟。 */
    __HAL_RCC_ETH_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    /**ETH GPIO Configuration
    PC1     ------> ETH_MDC
    PA1     ------> ETH_REF_CLK
    PA2     ------> ETH_MDIO
    PA7     ------> ETH_CRS_DV
    PC4     ------> ETH_RXD0
    PC5     ------> ETH_RXD1
    PG11     ------> ETH_TX_EN
    PG13     ------> ETH_TXD0
    PG14     ------> ETH_TXD1
    */
    /* RMII 信号线统一配置为 AF11_ETH，速度设为 VERY_HIGH 以满足 50MHz REF_CLK。 */
    /* PC1/PC4/PC5 分别用于 MDC、RXD0、RXD1。 */
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* PA1/PA2/PA7 分别用于 REF_CLK、MDIO、CRS_DV。 */
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PG11/PG13/PG14 分别用于 TX_EN、TXD0、TXD1。 */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* Peripheral interrupt init */
    /* ETH 中断用于接收、发送完成和异常事件，优先级低于 SDIO DMA。 */
    /* ETH_IRQn 是普通以太网中断。 */
    HAL_NVIC_SetPriority(ETH_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
    /* ETH_WKUP_IRQn 是以太网唤醒中断。 */
    HAL_NVIC_SetPriority(ETH_WKUP_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(ETH_WKUP_IRQn);
  /* USER CODE BEGIN ETH_MspInit 1 */

  /* USER CODE END ETH_MspInit 1 */
  }
}

/**
  * @brief  反初始化 ETH 外设底层硬件资源。
  * @param  ethHandle ETH HAL 句柄。
  * @retval None
  * @note   当前工程正常运行时很少调用，主要供 HAL_DeInit 或错误恢复流程使用。
  */
void HAL_ETH_MspDeInit(ETH_HandleTypeDef* ethHandle)
{
  /* 只处理 ETH 外设实例。 */
  if(ethHandle->Instance==ETH)
  {
  /* USER CODE BEGIN ETH_MspDeInit 0 */

  /* USER CODE END ETH_MspDeInit 0 */
    /* Peripheral clock disable */
    /* 关闭 ETH 外设时钟。 */
    __HAL_RCC_ETH_CLK_DISABLE();

    /**ETH GPIO Configuration
    PC1     ------> ETH_MDC
    PA1     ------> ETH_REF_CLK
    PA2     ------> ETH_MDIO
    PA7     ------> ETH_CRS_DV
    PC4     ------> ETH_RXD0
    PC5     ------> ETH_RXD1
    PG11     ------> ETH_TX_EN
    PG13     ------> ETH_TXD0
    PG14     ------> ETH_TXD1
    */
    /* 释放 PC 上 ETH 复用引脚。 */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5);

    /* 释放 PA 上 ETH 复用引脚。 */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7);

    /* 释放 PG 上 ETH 复用引脚。 */
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_14);

    /* Peripheral interrupt Deinit*/
    /* 关闭 ETH 普通中断。 */
    HAL_NVIC_DisableIRQ(ETH_IRQn);

    /* 关闭 ETH 唤醒中断。 */
    HAL_NVIC_DisableIRQ(ETH_WKUP_IRQn);

  /* USER CODE BEGIN ETH_MspDeInit 1 */

  /* USER CODE END ETH_MspDeInit 1 */
  }
}

/*******************************************************************************
                       PHI IO Functions
*******************************************************************************/
/**
  * @brief  初始化 PHY 管理接口 MDIO/MDC。
  * @retval 0 表示成功；当前实现不会返回失败。
  * @note   GPIO 已在 HAL_ETH_MspInit() 中配置，这里只设置 MDIO 时钟分频。
  */
int32_t ETH_PHY_IO_Init(void)
{
  /* We assume that MDIO GPIO configuration is already done
     in the ETH_MspInit() else it should be done here
  */

  /* Configure the MDIO Clock */
  /* 根据当前 HCLK 设置 MDC 分频，保证 MDIO 管理时钟不超出 PHY 规格。 */
  HAL_ETH_SetMDIOClockRange(&heth);

  /* LAN8742 驱动约定 0 表示底层 IO 初始化成功。 */
  return 0;
}

/**
  * @brief  反初始化 PHY 管理接口。
  * @retval 0 表示成功。
  * @note   当前 MDIO/MDC 资源随 ETH MSP 统一释放，因此这里无需额外操作。
  */
int32_t ETH_PHY_IO_DeInit (void)
{
  /* 保留空实现以匹配 LAN8742_IOCtx 回调接口。 */
  return 0;
}

/**
  * @brief  通过 MDIO 读取 PHY 寄存器。
  * @param  DevAddr PHY 地址，由 LAN8742_Init 扫描得到。
  * @param  RegAddr PHY 寄存器地址。
  * @param  pRegVal 输出参数，保存读取到的寄存器值。
  * @retval 0 表示成功，-1 表示 HAL 读取失败。
  */
int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  /* 通过 MDIO 读取 PHY 寄存器，例如链路状态、速率、双工模式。 */
  if(HAL_ETH_ReadPHYRegister(&heth, DevAddr, RegAddr, pRegVal) != HAL_OK)
  {
    /* HAL 返回错误时转换成 LAN8742 驱动约定的 -1。 */
    return -1;
  }

  /* 成功读取寄存器。 */
  return 0;
}

/**
  * @brief  通过 MDIO 写 PHY 寄存器。
  * @param  DevAddr PHY 地址。
  * @param  RegAddr PHY 寄存器地址。
  * @param  RegVal  要写入的寄存器值。
  * @retval 0 表示成功，-1 表示 HAL 写入失败。
  */
int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  /* 通过 MDIO 写 PHY 寄存器，例如复位、自动协商等控制位。 */
  if(HAL_ETH_WritePHYRegister(&heth, DevAddr, RegAddr, RegVal) != HAL_OK)
  {
    /* 写寄存器失败时通知 PHY 驱动。 */
    return -1;
  }

  /* 成功写入寄存器。 */
  return 0;
}

/**
  * @brief  获取 PHY 驱动内部延时/超时使用的毫秒时间。
  * @retval 当前 HAL tick，单位 ms。
  */
int32_t ETH_PHY_IO_GetTick(void)
{
  /* LAN8742 驱动使用该时间基处理复位、自动协商等超时。 */
  return HAL_GetTick();
}

/**
  * @brief  将 PHY 链路状态枚举转换成可读字符串。
  * @param  state LAN8742_GetLinkState() 返回的状态值。
  * @retval 指向常量字符串的指针。
  */
static const char *ETH_PhyLinkStateText(int32_t state)
{
  /* 将 LAN8742_GetLinkState() 返回值转换为串口日志可读文本。 */
  switch (state)
  {
  case LAN8742_STATUS_LINK_DOWN:
    return "down";
  case LAN8742_STATUS_100MBITS_FULLDUPLEX:
    return "100M full";
  case LAN8742_STATUS_100MBITS_HALFDUPLEX:
    return "100M half";
  case LAN8742_STATUS_10MBITS_FULLDUPLEX:
    return "10M full";
  case LAN8742_STATUS_10MBITS_HALFDUPLEX:
    return "10M half";
  case LAN8742_STATUS_AUTONEGO_NOTDONE:
    return "autoneg not done";
  case LAN8742_STATUS_READ_ERROR:
    return "read error";
  default:
    return "unknown";
  }
}

#if (ETH_DEBUG_VERBOSE != 0U)
/**
  * @brief  周期打印 PHY 寄存器、链路状态和 ETH 收发计数。
  * @note   默认关闭。需要深度排查链路问题时，把 ETH_DEBUG_VERBOSE 设为 1。
  */
static void ETH_PrintPhyDebug(void)
{
  /* last_print_tick 控制打印周期，避免串口日志刷屏。 */
  static uint32_t last_print_tick = 0U;
  /* now 保存当前系统 tick。 */
  uint32_t now = HAL_GetTick();
  /* 下列变量分别保存 PHY 常用寄存器。 */
  uint32_t bcr = 0U;
  uint32_t bsr = 0U;
  uint32_t phyid1 = 0U;
  uint32_t phyid2 = 0U;
  uint32_t physcsr = 0U;
  /* link_state 保存 PHY 驱动解析出的链路状态。 */
  int32_t link_state = 0;
  /* r0-r4 保存每次 MDIO 读取是否成功。 */
  int32_t r0 = 0;
  int32_t r1 = 0;
  int32_t r2 = 0;
  int32_t r3 = 0;
  int32_t r4 = 0;

  if ((now - last_print_tick) < 1000U)
  {
    /* 未到 1 秒打印周期，直接返回。 */
    return;
  }
  /* 更新时间戳，下次至少 1 秒后再打印。 */
  last_print_tick = now;

  /* PHY 地址无效或驱动未初始化时，不能读取寄存器。 */
  if ((LAN8742.DevAddr > ETH_PHY_MAX_DEV_ADDR) || (LAN8742.Is_Initialized == 0U))
  {
    printf("[PHY] no valid PHY address, dev=%lu initialized=%lu\r\n",
           (unsigned long)LAN8742.DevAddr,
           (unsigned long)LAN8742.Is_Initialized);
    return;
  }

  /* 分别读取控制、状态、芯片 ID 和特殊控制状态寄存器。 */
  r0 = ETH_PHY_IO_ReadReg(LAN8742.DevAddr, LAN8742_BCR, &bcr);
  r1 = ETH_PHY_IO_ReadReg(LAN8742.DevAddr, LAN8742_BSR, &bsr);
  r2 = ETH_PHY_IO_ReadReg(LAN8742.DevAddr, LAN8742_PHYI1R, &phyid1);
  r3 = ETH_PHY_IO_ReadReg(LAN8742.DevAddr, LAN8742_PHYI2R, &phyid2);
  r4 = ETH_PHY_IO_ReadReg(LAN8742.DevAddr, LAN8742_PHYSCSR, &physcsr);
  link_state = LAN8742_GetLinkState(&LAN8742);

  /* 任意寄存器读取失败时打印失败信息并退出，避免使用无效寄存器值。 */
  if ((r0 < 0) || (r1 < 0) || (r2 < 0) || (r3 < 0) || (r4 < 0))
  {
    printf("[PHY] read failed dev=%lu r=%ld,%ld,%ld,%ld,%ld state=%ld(%s)\r\n",
           (unsigned long)LAN8742.DevAddr,
           (long)r0, (long)r1, (long)r2, (long)r3, (long)r4,
           (long)link_state, ETH_PhyLinkStateText(link_state));
    return;
  }

  /* 打印 PHY 寄存器和链路状态。 */
  printf("[PHY] dev=%lu BCR=0x%04lX BSR=0x%04lX PHYID=0x%04lX%04lX PHYSCSR=0x%04lX state=%ld(%s)\r\n",
         (unsigned long)LAN8742.DevAddr,
         (unsigned long)bcr,
         (unsigned long)bsr,
         (unsigned long)phyid1,
         (unsigned long)phyid2,
         (unsigned long)physcsr,
         (long)link_state,
         ETH_PhyLinkStateText(link_state));
  /* 打印软件统计的 ETH 收发计数。 */
  printf("[ETH] cnt rx=%lu tx=%lu arp_rx=%lu arp_tx=%lu ip_rx=%lu ip_tx=%lu\r\n",
         (unsigned long)EthRxPacketCount,
         (unsigned long)EthTxPacketCount,
         (unsigned long)EthRxArpCount,
         (unsigned long)EthTxArpCount,
         (unsigned long)EthRxIpCount,
         (unsigned long)EthTxIpCount);
  /* 记录 MAC/DMA 关键寄存器，便于和 DMA 状态文本一起分析。 */
  EthLastMacCr = heth.Instance->MACCR;
  EthLastDmaOmr = heth.Instance->DMAOMR;
  EthLastDmaSr = heth.Instance->DMASR;
  printf("[ETH] reg MACCR=0x%08lX DMAOMR=0x%08lX DMASR=0x%08lX gState=%lu err=0x%08lX dmaerr=0x%08lX\r\n",
         (unsigned long)EthLastMacCr,
         (unsigned long)EthLastDmaOmr,
         (unsigned long)EthLastDmaSr,
         (unsigned long)heth.gState,
         (unsigned long)heth.ErrorCode,
         (unsigned long)heth.DMAErrorCode);
  /* 进一步打印 DMA 状态位和当前描述符内容。 */
  ETH_PrintDmaStatus(EthLastDmaSr);
  ETH_PrintDescDebug();
}
#endif

/**
  * @brief  检查 PHY 链路状态，并同步更新 ETH MAC 与 LwIP netif 状态。
  * @param  netif LwIP 网卡对象。
  * @retval None
  * @note   该函数由 MX_LWIP_Process() 周期调用，用于处理网线插拔、速率和双工变化。
  */
void ethernet_link_check_state(struct netif *netif)
{
  /* MACConf 保存当前 MAC 配置，链路 up 时会修改速率和双工模式。 */
  ETH_MACConfigTypeDef MACConf = {0};
  /* PHYLinkState 保存 PHY 当前链路状态。 */
  int32_t PHYLinkState = 0;
  /* linkchanged 表示是否得到可应用的新速率/双工配置。 */
  uint32_t linkchanged = 0U, speed = 0U, duplex = 0U;

#if (ETH_DEBUG_VERBOSE != 0U)
  ETH_PrintPhyDebug();
#endif
  /* PHY 尚未初始化或地址无效时，强制关闭 netif，避免上层继续发包。 */
  if ((LAN8742.DevAddr > ETH_PHY_MAX_DEV_ADDR) || (LAN8742.Is_Initialized == 0U))
  {
    if (netif_is_link_up(netif))
    {
      /* 停止 ETH 中断收发，避免 PHY 不可用时继续访问硬件链路。 */
      HAL_ETH_Stop(&heth);
      /* netif_set_down 表示网卡管理状态不可用。 */
      netif_set_down(netif);
      /* netif_set_link_down 表示物理链路断开。 */
      netif_set_link_down(netif);
    }
    return;
  }

  /* 读取 PHY 自动协商后的链路状态。 */
  PHYLinkState = LAN8742_GetLinkState(&LAN8742);

  /* 网线断开：停止 ETH 中断收发，并把 LwIP 网卡状态置为 down/link down。 */
  if(netif_is_link_up(netif) && (PHYLinkState <= LAN8742_STATUS_LINK_DOWN))
  {
    HAL_ETH_Stop(&heth);
    ETH_StopArpAnnounce();
    netif_set_down(netif);
    netif_set_link_down(netif);
    printf("[ETH] link down\r\n");
  }
  else if(!netif_is_link_up(netif) && (PHYLinkState > LAN8742_STATUS_LINK_DOWN))
  {
    /* 网线接入：根据 PHY 自动协商结果配置 MAC 的速率和双工模式。 */
    switch (PHYLinkState)
    {
    case LAN8742_STATUS_100MBITS_FULLDUPLEX:
      /* 100M 全双工。 */
      duplex = ETH_FULLDUPLEX_MODE;
      speed = ETH_SPEED_100M;
      linkchanged = 1;
      break;
    case LAN8742_STATUS_100MBITS_HALFDUPLEX:
      /* 100M 半双工。 */
      duplex = ETH_HALFDUPLEX_MODE;
      speed = ETH_SPEED_100M;
      linkchanged = 1;
      break;
    case LAN8742_STATUS_10MBITS_FULLDUPLEX:
      /* 10M 全双工。 */
      duplex = ETH_FULLDUPLEX_MODE;
      speed = ETH_SPEED_10M;
      linkchanged = 1;
      break;
    case LAN8742_STATUS_10MBITS_HALFDUPLEX:
      /* 10M 半双工。 */
      duplex = ETH_HALFDUPLEX_MODE;
      speed = ETH_SPEED_10M;
      linkchanged = 1;
      break;
    default:
      /* 自动协商未完成或读取异常时，不改变 MAC 配置。 */
      break;
    }

    if(linkchanged)
    {
      /* Get MAC Config MAC */
      /* 先读取当前 MAC 配置，再只修改速率和双工字段。 */
      HAL_ETH_GetMACConfig(&heth, &MACConf);
      MACConf.DuplexMode = duplex;
      MACConf.Speed = speed;
      HAL_ETH_SetMACConfig(&heth, &MACConf);
      /* MAC 配置完成后启动 ETH 收发；本工程用 MX_LWIP_Process() 裸机轮询收包。 */
      HAL_ETH_Start(&heth);


      netif_set_up(netif);
      netif_set_link_up(netif);
      ETH_StartArpAnnounce();
      /* 串口提示当前链路状态，便于用户确认网线和交换机协商结果。 */
      printf("[ETH] link up: %s %s duplex\r\n",
             (speed == ETH_SPEED_100M) ? "100M" : "10M",
             (duplex == ETH_FULLDUPLEX_MODE) ? "full" : "half");
      printf("[ETH] ready: ping 192.168.10.123, UDP echo port 5000\r\n");
    }
  }

  ETH_ProcessArpAnnounce(netif);

}

/**
  * @brief  ETH HAL 接收缓冲分配回调。
  * @param  buff 输出参数，返回给 ETH DMA 使用的接收数据缓冲地址。
  * @retval None
  * @note   HAL 在构建 RX 描述符时调用该函数；这里从 LwIP RX_POOL 分配零拷贝 pbuf。
  */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
/* USER CODE BEGIN HAL ETH RxAllocateCallback */
  /* ETH HAL 需要新的 DMA 接收缓冲时，从 RX_POOL 分配一个自定义 pbuf。 */
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
  if (p)
  {
    /* Get the buff from the struct pbuf address. */
    /* RxBuff_t 中 buff 字段是真正给 DMA 写入帧数据的内存区域。 */
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    /* 绑定自定义释放函数，LwIP 释放 pbuf 时会回到 pbuf_free_custom。 */
    p->custom_free_function = pbuf_free_custom;
    /* Initialize the struct pbuf.
    * This must be performed whenever a buffer's allocated because it may be
    * changed by lwIP or the app, e.g., pbuf_free decrements ref. */
    /* 把已有 DMA buffer 包装成 LwIP pbuf，不再额外拷贝数据。 */
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUF_SIZE);
  }
  else
  {
    /* 池耗尽时暂时不给 DMA 新缓冲，等 pbuf_free_custom 归还后再恢复。 */
    RxAllocStatus = RX_ALLOC_ERROR;
    *buff = NULL;
  }
/* USER CODE END HAL ETH RxAllocateCallback */
}

/**
  * @brief  ETH HAL 接收 buffer 链接回调。
  * @param  pStart  输出/输入参数，指向当前接收帧 pbuf 链表头。
  * @param  pEnd    输出/输入参数，指向当前接收帧 pbuf 链表尾。
  * @param  buff    DMA 写入数据的 buffer 地址。
  * @param  Length  当前 buffer 中有效数据长度。
  * @retval None
  * @note   一帧以太网数据可能跨多个 DMA buffer，本函数负责把它们串成 pbuf 链。
  */
void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
/* USER CODE BEGIN HAL ETH RxLinkCallback */

  /* 将 HAL 的 void** 转成 LwIP pbuf 链表头尾指针。 */
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd = (struct pbuf **)pEnd;
  /* p 表示当前 buff 对应的 pbuf。 */
  struct pbuf *p = NULL;

  /* Get the struct pbuf from the buff address. */
  /* 根据 DMA 缓冲地址反推出对应的 pbuf 结构。 */
  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  /* 当前 pbuf 暂时作为链尾，next 先清空。 */
  p->next = NULL;
  /* tot_len 后面统一累加，这里先归零。 */
  p->tot_len = 0;
  /* len 是当前 DMA buffer 片段的有效长度。 */
  p->len = Length;

  /* Chain the buffer. */
  /* 一帧可能由多个 DMA buffer 组成，这里把多个 pbuf 串成链表。 */
  if (!*ppStart)
  {
    /* The first buffer of the packet. */
    /* 如果链表还没有头节点，当前 pbuf 就是整帧的第一个片段。 */
    *ppStart = p;
  }
  else
  {
    /* Chain the buffer to the end of the packet. */
    /* 如果已经有片段，则把当前 pbuf 接到原链尾后面。 */
    (*ppEnd)->next = p;
  }
  /* 更新链尾指针。 */
  *ppEnd  = p;

  /* Update the total length of all the buffers of the chain. Each pbuf in the chain should have its tot_len
   * set to its own length, plus the length of all the following pbufs in the chain. */
  for (p = *ppStart; p != NULL; p = p->next)
  {
    /* LwIP 要求链中每个 pbuf 的 tot_len 等于自己到链尾的总长度。
     * 这里沿用 CubeMX 生成逻辑，把当前片段长度累加到链上每个节点。
     */
    p->tot_len += Length;
  }

/* USER CODE END HAL ETH RxLinkCallback */
}

/**
  * @brief  ETH HAL 发送完成释放回调。
  * @param  buff low_level_output() 中挂到 CurrentPacketAddress 的 pbuf 指针。
  * @retval None
  * @note   发送路径中对 pbuf_ref() 增加的引用，最终在这里释放。
  */
void HAL_ETH_TxFreeCallback(uint32_t * buff)
{
/* USER CODE BEGIN HAL ETH TxFreeCallback */

  /* ETH DMA 发送完成后释放当初挂在 TxConfig.pData 上的 pbuf。 */
  pbuf_free((struct pbuf *)buff);

/* USER CODE END HAL ETH TxFreeCallback */
}

/* USER CODE BEGIN 8 */

/* USER CODE END 8 */
