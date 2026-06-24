/* USER CODE BEGIN Header */
/* USER CODE END Header */

#include "tcp_client.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define TCP_CLIENT_SERVER_IP0 192U
#define TCP_CLIENT_SERVER_IP1 168U
#define TCP_CLIENT_SERVER_IP2 10U
#define TCP_CLIENT_SERVER_IP3 100U
#define TCP_CLIENT_SERVER_PORT 1883U
#define TCP_CLIENT_RX_MAX_LEN 128U
#define TCP_CLIENT_CONNECT_DELAY_MS 3000U

extern struct netif gnetif;

typedef enum
{
  TCP_CLIENT_STATE_IDLE = 0,
  TCP_CLIENT_STATE_CONNECTING,
  TCP_CLIENT_STATE_CONNECTED,
  TCP_CLIENT_STATE_CLOSED,
  TCP_CLIENT_STATE_ERROR
} TCP_Client_State;

static struct tcp_pcb *TcpClientPcb;
static TCP_Client_State TcpClientState = TCP_CLIENT_STATE_IDLE;
static uint8_t TcpClientStarted;
static uint8_t TcpClientConnectAttempted;
static uint32_t TcpClientStartTick;

static err_t TCP_Client_Connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t TCP_Client_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void TCP_Client_Error(void *arg, err_t err);

static void TCP_Client_Close(void)
{
  err_t close_err;

  if (TcpClientPcb == NULL)
  {
    return;
  }

  tcp_arg(TcpClientPcb, NULL);
  tcp_recv(TcpClientPcb, NULL);
  tcp_err(TcpClientPcb, NULL);

  close_err = tcp_close(TcpClientPcb);
  if (close_err != ERR_OK)
  {
    tcp_abort(TcpClientPcb);
  }

  TcpClientPcb = NULL;
}

static void TCP_Client_StartConnect(void)
{
  ip_addr_t server_ip;
  err_t err;

  if (TcpClientConnectAttempted != 0U)
  {
    return;
  }

  if (!netif_is_link_up(&gnetif))
  {
    return;
  }

  if ((HAL_GetTick() - TcpClientStartTick) < TCP_CLIENT_CONNECT_DELAY_MS)
  {
    return;
  }

  TcpClientConnectAttempted = 1U;

  TcpClientPcb = tcp_new();
  if (TcpClientPcb == NULL)
  {
    TcpClientState = TCP_CLIENT_STATE_ERROR;
    printf("[TCP-CLIENT] create pcb failed\r\n");
    return;
  }

  tcp_arg(TcpClientPcb, NULL);
  tcp_recv(TcpClientPcb, TCP_Client_Recv);
  tcp_err(TcpClientPcb, TCP_Client_Error);

  IP4_ADDR(ip_2_ip4(&server_ip),
           TCP_CLIENT_SERVER_IP0,
           TCP_CLIENT_SERVER_IP1,
           TCP_CLIENT_SERVER_IP2,
           TCP_CLIENT_SERVER_IP3);

  TcpClientState = TCP_CLIENT_STATE_CONNECTING;
  printf("[TCP-CLIENT] connect to %u.%u.%u.%u:%u\r\n",
         (unsigned int)TCP_CLIENT_SERVER_IP0,
         (unsigned int)TCP_CLIENT_SERVER_IP1,
         (unsigned int)TCP_CLIENT_SERVER_IP2,
         (unsigned int)TCP_CLIENT_SERVER_IP3,
         (unsigned int)TCP_CLIENT_SERVER_PORT);

  err = tcp_connect(TcpClientPcb, &server_ip, TCP_CLIENT_SERVER_PORT, TCP_Client_Connected);
  if (err != ERR_OK)
  {
    printf("[TCP-CLIENT] tcp_connect failed: err=%ld\r\n", (long)err);
    TCP_Client_Close();
    TcpClientState = TCP_CLIENT_STATE_ERROR;
  }
}

static err_t TCP_Client_Connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
  static const char hello[] = "hello broker";
  err_t write_err;

  LWIP_UNUSED_ARG(arg);

  if ((err != ERR_OK) || (tpcb == NULL))
  {
    TcpClientState = TCP_CLIENT_STATE_ERROR;
    printf("[TCP-CLIENT] connected callback err=%ld\r\n", (long)err);
    return err;
  }

  TcpClientPcb = tpcb;
  TcpClientState = TCP_CLIENT_STATE_CONNECTED;
  printf("[TCP-CLIENT] connected\r\n");

  write_err = tcp_write(tpcb, hello, (u16_t)strlen(hello), TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK)
  {
    (void)tcp_output(tpcb);
    printf("[TCP-CLIENT] tx: %s\r\n", hello);
    return ERR_OK;
  }

  printf("[TCP-CLIENT] write failed: err=%ld\r\n", (long)write_err);
  TcpClientState = TCP_CLIENT_STATE_ERROR;
  return write_err;
}

static err_t TCP_Client_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  char text[TCP_CLIENT_RX_MAX_LEN];
  u16_t copy_len;

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
    printf("[TCP-CLIENT] server closed\r\n");
    TCP_Client_Close();
    TcpClientState = TCP_CLIENT_STATE_CLOSED;
    return ERR_OK;
  }

  if (err != ERR_OK)
  {
    pbuf_free(p);
    TcpClientState = TCP_CLIENT_STATE_ERROR;
    return err;
  }

  copy_len = p->tot_len;
  if (copy_len >= sizeof(text))
  {
    copy_len = (u16_t)(sizeof(text) - 1U);
  }

  (void)pbuf_copy_partial(p, text, copy_len, 0U);
  text[copy_len] = '\0';
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  printf("[TCP-CLIENT] rx: %s\r\n", text);
  return ERR_OK;
}

static void TCP_Client_Error(void *arg, err_t err)
{
  LWIP_UNUSED_ARG(arg);
  TcpClientPcb = NULL;
  TcpClientState = TCP_CLIENT_STATE_ERROR;
  printf("[TCP-CLIENT] error: err=%ld\r\n", (long)err);
}

void TCP_Client_Init(void)
{
  TcpClientStarted = 1U;
  TcpClientConnectAttempted = 0U;
  TcpClientStartTick = HAL_GetTick();
  TcpClientState = TCP_CLIENT_STATE_IDLE;
  printf("[TCP-CLIENT] test client ready\r\n");
}

void TCP_Client_Process(void)
{
  if (TcpClientStarted == 0U)
  {
    return;
  }

  if (TcpClientState == TCP_CLIENT_STATE_IDLE)
  {
    TCP_Client_StartConnect();
  }
}
