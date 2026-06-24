/* USER CODE BEGIN Header */
/* USER CODE END Header */

#include "http_server.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include <stdio.h>
#include <string.h>

#define HTTP_SERVER_PORT 80U
#define HTTP_REQUEST_MAX_LEN 256U
#define HTTP_RESPONSE_MAX_LEN 768U

static struct tcp_pcb *HttpListenPcb;

static err_t HTTP_Server_Accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t HTTP_Server_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void HTTP_Server_Error(void *arg, err_t err);

static const char HttpIndexBody[] =
  "<!doctype html>"
  "<html>"
  "<head>"
  "<meta charset=\"utf-8\">"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<title>STM32 Sensor Dashboard</title>"
  "</head>"
  "<body>"
  "<h1>STM32 Sensor Dashboard</h1>"
  "<p>Temperature: 25.3</p>"
  "<p>Humidity: 60.0</p>"
  "<p>Light: 1234</p>"
  "</body>"
  "</html>";

static const char HttpNotFoundBody[] =
  "<!doctype html>"
  "<html>"
  "<head><meta charset=\"utf-8\"><title>404 Not Found</title></head>"
  "<body><h1>404 Not Found</h1></body>"
  "</html>";

/**
  * @brief 把 HTTP 请求复制成以 '\0' 结尾的字符串。
  */
static u16_t HTTP_CopyRequestToText(const struct pbuf *p, char *text, u16_t text_size)
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
  return copy_len;
}

/**
  * @brief 从 HTTP 请求第一行提取路径。
  */
static void HTTP_GetRequestPath(const char *request, char *path, size_t path_size)
{
  const char *start;
  const char *end;
  size_t path_len;

  if ((request == NULL) || (path == NULL) || (path_size == 0U))
  {
    return;
  }

  path[0] = '\0';
  if (strncmp(request, "GET ", 4U) != 0)
  {
    return;
  }

  start = request + 4U;
  end = strchr(start, ' ');
  if (end == NULL)
  {
    return;
  }

  path_len = (size_t)(end - start);
  if (path_len >= path_size)
  {
    path_len = path_size - 1U;
  }

  (void)memcpy(path, start, path_len);
  path[path_len] = '\0';
}

/**
  * @brief 组装 HTTP 响应。
  */
static int HTTP_BuildResponse(const char *path, char *response, size_t response_size)
{
  const char *status;
  const char *body;

  if ((path == NULL) || (response == NULL) || (response_size == 0U))
  {
    return 0;
  }

  if (strcmp(path, "/") == 0)
  {
    status = "200 OK";
    body = HttpIndexBody;
  }
  else
  {
    status = "404 Not Found";
    body = HttpNotFoundBody;
  }

  return snprintf(response, response_size,
                  "HTTP/1.1 %s\r\n"
                  "Content-Type: text/html; charset=utf-8\r\n"
                  "Content-Length: %u\r\n"
                  "Connection: close\r\n"
                  "\r\n"
                  "%s",
                  status,
                  (unsigned int)strlen(body),
                  body);
}

/**
  * @brief 关闭 HTTP TCP 连接。
  */
static err_t HTTP_Server_Close(struct tcp_pcb *tpcb)
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
  * @brief 浏览器建立 TCP 连接后触发。
  */
static err_t HTTP_Server_Accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  LWIP_UNUSED_ARG(arg);

  if ((err != ERR_OK) || (newpcb == NULL))
  {
    return ERR_VAL;
  }

  tcp_arg(newpcb, NULL);
  tcp_recv(newpcb, HTTP_Server_Recv);
  tcp_err(newpcb, HTTP_Server_Error);

  printf("[HTTP] client connected\r\n");
  return ERR_OK;
}

/**
  * @brief 收到浏览器 HTTP 请求后触发。
  */
static err_t HTTP_Server_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  char request[HTTP_REQUEST_MAX_LEN];
  char path[32];
  char response[HTTP_RESPONSE_MAX_LEN];
  int response_len;
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
    printf("[HTTP] client closed\r\n");
    return HTTP_Server_Close(tpcb);
  }

  if (err != ERR_OK)
  {
    pbuf_free(p);
    return err;
  }

  (void)HTTP_CopyRequestToText(p, request, (u16_t)sizeof(request));
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  HTTP_GetRequestPath(request, path, sizeof(path));
  response_len = HTTP_BuildResponse(path, response, sizeof(response));
  if ((response_len <= 0) || (response_len >= (int)sizeof(response)))
  {
    printf("[HTTP] response build failed\r\n");
    return HTTP_Server_Close(tpcb);
  }

  write_err = tcp_write(tpcb, response, (u16_t)response_len, TCP_WRITE_FLAG_COPY);
  if (write_err != ERR_OK)
  {
    printf("[HTTP] write failed: err=%ld\r\n", (long)write_err);
    return write_err;
  }

  (void)tcp_output(tpcb);
  printf("[HTTP] request: GET %s\r\n", path[0] == '\0' ? "(unsupported)" : path);
  printf("[HTTP] response sent\r\n");

  return HTTP_Server_Close(tpcb);
}

/**
  * @brief HTTP 连接异常关闭时触发。
  */
static void HTTP_Server_Error(void *arg, err_t err)
{
  LWIP_UNUSED_ARG(arg);
  printf("[HTTP] connection error: err=%ld\r\n", (long)err);
}

/**
  * @brief 初始化 HTTP 服务。
  */
void HTTP_Server_Init(void)
{
  struct tcp_pcb *pcb;
  struct tcp_pcb *listen_pcb;
  err_t err;

  if (HttpListenPcb != NULL)
  {
    return;
  }

  pcb = tcp_new();
  if (pcb == NULL)
  {
    printf("[HTTP] server init failed: no pcb\r\n");
    return;
  }

  err = tcp_bind(pcb, IP_ADDR_ANY, HTTP_SERVER_PORT);
  if (err != ERR_OK)
  {
    printf("[HTTP] bind failed: err=%ld\r\n", (long)err);
    (void)tcp_close(pcb);
    return;
  }

  listen_pcb = tcp_listen(pcb);
  if (listen_pcb == NULL)
  {
    printf("[HTTP] listen failed\r\n");
    (void)tcp_close(pcb);
    return;
  }

  HttpListenPcb = listen_pcb;
  tcp_accept(HttpListenPcb, HTTP_Server_Accept);

  printf("[HTTP] server listen on port %u\r\n", (unsigned int)HTTP_SERVER_PORT);
}
