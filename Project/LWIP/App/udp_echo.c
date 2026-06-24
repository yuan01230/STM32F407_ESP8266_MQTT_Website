/* USER CODE BEGIN Header */
/* USER CODE END Header */

#include "udp_echo.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "../../Library/led/led.h"
#include <stdio.h>
#include <string.h>

/* UDP 回显服务监听端口：电脑向 192.168.10.123:5000 发 UDP 数据，开发板会原样回发。 */
#define UDP_ECHO_PORT 5000U
#define UDP_CMD_MAX_LEN 64U
#define UDP_REPLY_MAX_LEN 128U

/* UDP 协议控制块，保存本地端口、回调函数等 LwIP UDP 状态。 */
static struct udp_pcb *UdpEchoPcb;

/**
  * @brief  将 UDP pbuf 中的 payload 复制成以 '\0' 结尾的 C 字符串。
  * @param  p         LwIP 收到的 pbuf，可能是单个 pbuf，也可能是 pbuf 链。
  * @param  text      目标字符串缓冲区。
  * @param  text_size 目标缓冲区大小，必须至少能容纳结尾的 '\0'。
  * @retval 实际复制的字节数，不包含结尾 '\0'。
  * @note   UDP payload 不保证自带字符串结束符，所以这里统一截断并补 '\0'。
  */
static u16_t UDP_CopyPayloadToText(const struct pbuf *p, char *text, u16_t text_size)
{
  /* copy_len 表示最终要从 pbuf 复制到 text 的字节数。 */
  u16_t copy_len;

  /* 防御性检查：任一输入无效时直接返回 0，调用方会得到空命令。 */
  if ((p == NULL) || (text == NULL) || (text_size == 0U))
  {
    return 0U;
  }

  /* p->tot_len 是整个 pbuf 链的总长度，比 p->len 更适合处理链式包。 */
  copy_len = p->tot_len;
  /* 为字符串结尾 '\0' 预留 1 个字节，避免写越界。 */
  if (copy_len >= text_size)
  {
    copy_len = (u16_t)(text_size - 1U);
  }

  /* pbuf_copy_partial 可以跨 pbuf 链复制数据，从 offset=0 开始拷贝命令文本。 */
  (void)pbuf_copy_partial((struct pbuf *)p, text, copy_len, 0U);
  /* 手动补字符串结束符，让后续 strcmp/snprintf 可以安全处理。 */
  text[copy_len] = '\0';
  return copy_len;
}

/**
  * @brief  根据收到的 UDP 文本命令生成回复字符串。
  * @param  cmd        已经以 '\0' 结尾的命令字符串。
  * @param  reply      回复缓冲区。
  * @param  reply_size 回复缓冲区大小。
  * @note   支持 Echo、ping、sensor 和 LED 控制等第三阶段实验命令。
  */
static void UDP_HandleCommand(const char *cmd, char *reply, size_t reply_size)
{
  /* 参数检查：输出缓冲区不可用时不能继续写入。 */
  if ((cmd == NULL) || (reply == NULL) || (reply_size == 0U))
  {
    return;
  }

  /* ping 用于最小连通性测试，PC 端发 ping，开发板回 pong。 */
  if (strcmp(cmd, "ping") == 0)
  {
    (void)snprintf(reply, reply_size, "pong");
  }
  /* hello stm32 用于确认 payload 字符串完整收发。 */
  else if (strcmp(cmd, "hello stm32") == 0)
  {
    (void)snprintf(reply, reply_size, "hello stm32");
  }
  /* sensor 先返回固定 JSON，验证“命令 -> 应用层响应”的完整路径。 */
  else if (strcmp(cmd, "sensor") == 0)
  {
    (void)snprintf(reply, reply_size, "{\"temp\":25.3,\"humi\":60.0,\"light\":1234}");
  }
  /* LED_On/LED_Off 已经在驱动层处理了高低电平有效性，这里只表达业务含义。 */
  else if (strcmp(cmd, "led on") == 0)
  {
    LED_On(LED0);
    (void)snprintf(reply, reply_size, "ok led on");
  }
  else if (strcmp(cmd, "led off") == 0)
  {
    LED_Off(LED0);
    (void)snprintf(reply, reply_size, "ok led off");
  }
  else if (strcmp(cmd, "led toggle") == 0)
  {
    LED_Toggle(LED0);
    (void)snprintf(reply, reply_size, "ok led toggle");
  }
  else
  {
    /* 未识别命令也返回文本，方便 PC 端看到开发板确实收到过数据。 */
    (void)snprintf(reply, reply_size, "err unknown command: %s", cmd);
  }
}

/**
  * @brief  UDP 收包回调函数。
  * @param  arg  用户参数，本工程未使用。
  * @param  pcb  当前 UDP 控制块。
  * @param  p    收到的数据包 pbuf，回调结束前必须释放。
  * @param  addr 远端 IP 地址。
  * @param  port 远端 UDP 端口。
  * @note   收到有效数据后直接调用 udp_sendto 原样发回，作为网络连通性测试。
  */
static void UDP_Echo_Receive(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port)
{
  /* cmd 保存收到的 UDP 命令文本，长度受 UDP_CMD_MAX_LEN 限制。 */
  char cmd[UDP_CMD_MAX_LEN];
  /* reply 保存准备回发给远端的响应文本，默认初始化为空串。 */
  char reply[UDP_REPLY_MAX_LEN] = "";
  /* reply_pbuf 是为回复数据单独申请的发送 pbuf。 */
  struct pbuf *reply_pbuf;

  /* 当前没有使用用户参数，显式标记可避免编译器告警。 */
  LWIP_UNUSED_ARG(arg);

  /* LwIP 回调参数异常时不能继续处理；如果 pbuf 已分配则必须释放。 */
  if ((pcb == NULL) || (p == NULL) || (addr == NULL))
  {
    /* pbuf 由 LwIP 分配，异常路径也要释放，避免接收缓冲泄漏。 */
    if (p != NULL)
    {
      pbuf_free(p);
    }
    return;
  }

  /* 原始 Echo 写法：不解析 payload，直接把收到的 pbuf 原样发回给发送方。
   * 现在先注释保留，便于和下面的“命令解析后回复”做对比。
   */
  /* (void)udp_sendto(pcb, p, addr, port); */

  /* 命令协议写法：先把 UDP payload 复制成 C 字符串，再根据命令生成回复。 */
  (void)UDP_CopyPayloadToText(p, cmd, (u16_t)sizeof(cmd));
  UDP_HandleCommand(cmd, reply, sizeof(reply));

  /* 为回复内容申请一个传输层 pbuf；使用 PBUF_RAM 方便写入字符串内容。 */
  reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, (u16_t)strlen(reply), PBUF_RAM);
  if (reply_pbuf != NULL)
  {
    /* 把 reply 字符串复制进 pbuf payload。 */
    (void)pbuf_take(reply_pbuf, reply, (u16_t)strlen(reply));
    /* 按原始发送方 IP 和端口回包。 */
    (void)udp_sendto(pcb, reply_pbuf, addr, port);
    /* 发送调用返回后释放回复 pbuf，LwIP 会处理内部引用计数。 */
    pbuf_free(reply_pbuf);
  }

  /* 收到的 pbuf 由回调负责释放；不释放会耗尽 RX 缓冲池。 */
  pbuf_free(p);
}

/**
  * @brief  初始化 UDP echo 服务。
  * @note   该函数在 MX_LWIP_Init() 末尾调用，只需初始化一次。
  */
void UDP_Echo_Init(void)
{
  /* err 保存 udp_bind 的返回值，用来判断端口绑定是否成功。 */
  err_t err;

  if (UdpEchoPcb != NULL)
  {
    /* 已初始化时直接返回，防止重复绑定同一个端口。 */
    return;
  }

  /* 创建 UDP PCB；失败通常表示 LwIP 内存池不足。 */
  UdpEchoPcb = udp_new();
  if (UdpEchoPcb == NULL)
  {
    printf("[UDP] echo init failed: no pcb\r\n");
    return;
  }

  /* 绑定到所有本地 IP 地址的 5000 端口。 */
  err = udp_bind(UdpEchoPcb, IP_ADDR_ANY, UDP_ECHO_PORT);
  if (err != ERR_OK)
  {
    printf("[UDP] echo bind failed: err=%ld\r\n", (long)err);
    udp_remove(UdpEchoPcb);
    UdpEchoPcb = NULL;
    return;
  }

  /* 注册收包回调。之后 MX_LWIP_Process() 把数据交给协议栈时会触发该函数。 */
  udp_recv(UdpEchoPcb, UDP_Echo_Receive, NULL);
  printf("[UDP] echo server listen on port %u\r\n", (unsigned int)UDP_ECHO_PORT);
}
