/* USER CODE BEGIN Header */
/* USER CODE END Header */

#include "tcp_server.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include <stdio.h>
#include <string.h>

#define TCP_SERVER_PORT 5001U
#define TCP_CMD_MAX_LEN 64U
#define TCP_REPLY_MAX_LEN 128U

static struct tcp_pcb *TcpListenPcb;

static err_t TCP_Server_Accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t TCP_Server_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void TCP_Server_Error(void *arg, err_t err);

/**
  * @brief 把 TCP pbuf 中的字节复制成 C 字符串。
  * @note  TCP 是字节流，第一版实验先假设一次 recv 收到一条短命令。
  */
static u16_t TCP_CopyPayloadToText(const struct pbuf *p, char *text, u16_t text_size)
{
  u16_t copy_len;

  if ((p == NULL) || (text == NULL) || (text_size == 0U))
  {
    return 0U;
  }

  copy_len = p->tot_len;
  if (copy_len >= text_size)
  {
    copy_len = (u16_t)(text_size - 1U);
  }

  (void)pbuf_copy_partial((struct pbuf *)p, text, copy_len, 0U);
  text[copy_len] = '\0';

  while ((copy_len > 0U) &&
         ((text[copy_len - 1U] == '\r') || (text[copy_len - 1U] == '\n')))
  {
    copy_len--;
    text[copy_len] = '\0';
  }

  return copy_len;
}

/**
  * @brief 根据 TCP 文本命令生成回复。
  */
static void TCP_HandleCommand(const char *cmd, char *reply, size_t reply_size)
{
  if ((cmd == NULL) || (reply == NULL) || (reply_size == 0U))
  {
    return;
  }

  if (strcmp(cmd, "ping") == 0)
  {
    (void)snprintf(reply, reply_size, "pong\r\n");
  }
  else if (strcmp(cmd, "hello stm32") == 0)
  {
    (void)snprintf(reply, reply_size, "hello stm32\r\n");
  }
  else if (strcmp(cmd, "sensor") == 0)
  {
    (void)snprintf(reply, reply_size, "temp=25.3,humi=60.0,light=1234\r\n");
  }
  else
  {
    (void)snprintf(reply, reply_size, "err unknown command\r\n");
  }
}

/**
  * @brief 关闭一条 TCP 连接。
  */
static err_t TCP_Server_Close(struct tcp_pcb *tpcb)
{
  err_t close_err;

  if (tpcb == NULL)
  {
    return ERR_ARG;
  }

  tcp_arg(tpcb, NULL);
  tcp_recv(tpcb, NULL);
  tcp_err(tpcb, NULL);

  close_err = tcp_close(tpcb);
  if (close_err != ERR_OK)
  {
    tcp_abort(tpcb);
    return ERR_ABRT;
  }

  return ERR_OK;
}

/**
  * @brief PC 建立 TCP 连接后触发。
  */
static err_t TCP_Server_Accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  LWIP_UNUSED_ARG(arg);

  if ((err != ERR_OK) || (newpcb == NULL))
  {
    return ERR_VAL;
  }

  tcp_arg(newpcb, NULL);
  tcp_recv(newpcb, TCP_Server_Recv);
  tcp_err(newpcb, TCP_Server_Error);

  printf("[TCP] client connected\r\n");
  return ERR_OK;
}

/**
  * @brief 收到 TCP 数据后触发。
  */
static err_t TCP_Server_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  char cmd[TCP_CMD_MAX_LEN];
  char reply[TCP_REPLY_MAX_LEN] = "";
  err_t write_err;

  LWIP_UNUSED_ARG(arg);

  if (tpcb == NULL)
  {
    if (p != NULL)
    {
      pbuf_free(p);
    }
    return ERR_ARG;
  }

  if (p == NULL)
  {
    printf("[TCP] client closed\r\n");
    return TCP_Server_Close(tpcb);
  }

  if (err != ERR_OK)
  {
    pbuf_free(p);
    return err;
  }

  (void)TCP_CopyPayloadToText(p, cmd, (u16_t)sizeof(cmd));
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  TCP_HandleCommand(cmd, reply, sizeof(reply));
  write_err = tcp_write(tpcb, reply, (u16_t)strlen(reply), TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK)
  {
    (void)tcp_output(tpcb);
    printf("[TCP] cmd=\"%s\" reply=\"%s\"\r\n", cmd, reply);
    return ERR_OK;
  }

  printf("[TCP] write failed: err=%ld\r\n", (long)write_err);
  return write_err;
}

/**
  * @brief TCP 连接异常关闭时触发。
  */
static void TCP_Server_Error(void *arg, err_t err)
{
  LWIP_UNUSED_ARG(arg);
  printf("[TCP] connection error: err=%ld\r\n", (long)err);
}

/**
  * @brief 初始化 TCP 命令服务器。
  */
void TCP_Server_Init(void)
{
  struct tcp_pcb *pcb;
  struct tcp_pcb *listen_pcb;
  err_t err;

  if (TcpListenPcb != NULL)
  {
    return;
  }

  pcb = tcp_new();
  if (pcb == NULL)
  {
    printf("[TCP] server init failed: no pcb\r\n");
    return;
  }

  err = tcp_bind(pcb, IP_ADDR_ANY, TCP_SERVER_PORT);
  if (err != ERR_OK)
  {
    printf("[TCP] bind failed: err=%ld\r\n", (long)err);
    (void)tcp_close(pcb);
    return;
  }

  listen_pcb = tcp_listen(pcb);
  if (listen_pcb == NULL)
  {
    printf("[TCP] listen failed\r\n");
    (void)tcp_close(pcb);
    return;
  }

  TcpListenPcb = listen_pcb;
  tcp_accept(TcpListenPcb, TCP_Server_Accept);

  printf("[TCP] command server listen on port %u\r\n", (unsigned int)TCP_SERVER_PORT);
}
