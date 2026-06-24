/* USER CODE BEGIN Header */
/* USER CODE END Header */

#include "mqtt_client.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "main.h"
#include "../../Library/led/led.h"
#include <stdio.h>
#include <string.h>

#define MQTT_BROKER_IP0 192U
#define MQTT_BROKER_IP1 168U
#define MQTT_BROKER_IP2 10U
#define MQTT_BROKER_IP3 100U
#define MQTT_BROKER_PORT 1883U
#define MQTT_CONNECT_DELAY_MS 4000U
#define MQTT_RECONNECT_DELAY_MS 3000U
#define MQTT_CONNACK_TIMEOUT_MS 5000U
#define MQTT_SUBACK_TIMEOUT_MS 5000U
#define MQTT_PING_INTERVAL_MS 30000U
#define MQTT_PUBLISH_INTERVAL_MS 10000U
#define MQTT_RX_MAX_LEN 128U
#define MQTT_CMD_MAX_LEN 64U
#define MQTT_PUBLISH_PACKET_MAX_LEN 192U
#define MQTT_PAYLOAD_MAX_LEN 128U

extern struct netif gnetif;

/**
  * @brief MQTT Client 状态机。
  * @note
  * - 第六天程序主要关注“能连接、能收发”。
  * - 第七天重构后，每个阶段都有明确状态，方便断线后重新回到连接流程。
  */
typedef enum
{
  MQTT_CLIENT_STATE_IDLE = 0,        /* 空闲：等待网口 link up 和启动延时。 */
  MQTT_CLIENT_STATE_TCP_CONNECTING,  /* 正在建立 TCP 连接。 */
  MQTT_CLIENT_STATE_WAIT_CONNACK,    /* TCP 已连接，已发送 CONNECT，等待 CONNACK。 */
  MQTT_CLIENT_STATE_WAIT_SUBACK,     /* 已发送 SUBSCRIBE，等待 SUBACK。 */
  MQTT_CLIENT_STATE_MQTT_CONNECTED,  /* MQTT 可用：允许 PUBLISH、PINGREQ 和接收命令。 */
  MQTT_CLIENT_STATE_RECONNECT_WAIT,  /* 断线/出错后的重连等待状态。 */
  MQTT_CLIENT_STATE_ERROR            /* 临时错误状态，下一轮会进入重连等待。 */
} MQTT_Client_State;

static struct tcp_pcb *MqttPcb;
static MQTT_Client_State MqttState = MQTT_CLIENT_STATE_IDLE;
static uint8_t MqttStarted;
static uint8_t MqttLinkWasUp;
static uint32_t MqttStartTick;
static uint32_t MqttStateTick;
static uint32_t MqttReconnectTick;
static uint32_t MqttLastPingTick;
static uint32_t MqttLastPublishTick;

/* MQTT 3.1.1 CONNECT, Client ID = "stm32-f407-eth", Clean Session = 1, KeepAlive = 60s. */
static const uint8_t MqttConnectPacket[] =
{
  0x10, 0x1A,
  0x00, 0x04, 'M', 'Q', 'T', 'T',
  0x04,
  0x02,
  0x00, 0x3C,
  0x00, 0x0E, 's', 't', 'm', '3', '2', '-', 'f', '4', '0', '7', '-', 'e', 't', 'h'
};

static const uint8_t MqttPingReqPacket[] = {0xC0, 0x00};

/* 周期上报主题：STM32 每 10 秒向该 topic 发布传感器数据和 LED 状态。 */
static const char MqttSensorTopic[] = "stm32/sensor";
/* 命令下发主题：MQTTX/PC 向该 topic 发布 led on/off/toggle。 */
static const char MqttCommandTopic[] = "stm32/cmd";
/* 状态回传主题：STM32 执行命令后立即向该 topic 发布执行后的 LED 状态。 */
static const char MqttStatusTopic[] = "stm32/status";
/*
 * MQTT 3.1.1 SUBSCRIBE 报文，订阅 topic = "stm32/cmd"，QoS = 0。
 * 0x82：固定报头，表示 SUBSCRIBE，QoS1 是协议要求。
 * 0x0E：Remaining Length = Packet ID 2 字节 + Topic Length 2 字节 + Topic 9 字节 + QoS 1 字节。
 */
static const uint8_t MqttSubscribePacket[] =
{
  0x82, 0x0E,
  0x00, 0x01,
  0x00, 0x09, 's', 't', 'm', '3', '2', '/', 'c', 'm', 'd',
  0x00
};

static err_t MQTT_Client_Connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t MQTT_Client_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void MQTT_Client_Error(void *arg, err_t err);
static void MQTT_Client_Close(void);
static void MQTT_Client_EnterReconnect(const char *reason);
static void MQTT_Client_CheckTimeout(void);
static void MQTT_Client_CheckLink(void);
static void MQTT_Client_StartConnect(void);
static void MQTT_Client_SendSubscribe(void);
static void MQTT_Client_HandlePublish(const uint8_t *data, u16_t len);
static void MQTT_Client_HandleCommand(const char *cmd);
static void MQTT_Client_SendPingReq(void);
static void MQTT_Client_SendPublish(void);

/**
  * @brief 发送一个 MQTT QoS0 文本 PUBLISH 报文。
  * @param topic   MQTT topic 字符串，例如 stm32/sensor 或 stm32/status。
  * @param payload MQTT payload 字符串，本实验中使用 JSON 文本。
  * @retval ERR_OK 发送成功；其它值表示参数、内存或 tcp_write 错误。
  * @note
  * - 本函数只实现当前学习阶段需要的“短报文 QoS0 PUBLISH”。
  * - Remaining Length 使用单字节编码，所以限制 remaining_len < 128。
  * - 组包格式：固定报头 0x30 + Remaining Length + Topic Length + Topic + Payload。
  * - tcp_write() 只是把数据放入 LwIP TCP 发送缓冲，随后调用 tcp_output() 立即推动发送。
  */
static err_t MQTT_Client_SendTextPublish(const char *topic, const char *payload)
{
  uint8_t packet[MQTT_PUBLISH_PACKET_MAX_LEN];
  u16_t topic_len;
  u16_t payload_len;
  u16_t remaining_len;
  u16_t idx;
  err_t write_err;

  if ((MqttPcb == NULL) || (MqttState != MQTT_CLIENT_STATE_MQTT_CONNECTED) ||
      (topic == NULL) || (payload == NULL))
  {
    return ERR_ARG;
  }

  topic_len = (u16_t)strlen(topic);
  payload_len = (u16_t)strlen(payload);
  /* PUBLISH 可变报头中 topic length 占 2 字节，所以 remaining_len 要额外加 2。 */
  remaining_len = (u16_t)(2U + topic_len + payload_len);
  if ((remaining_len >= 128U) || ((remaining_len + 2U) > sizeof(packet)))
  {
    printf("[MQTT] publish packet too large topic=%s\r\n", topic);
    return ERR_MEM;
  }

  idx = 0U;
  packet[idx++] = 0x30U;
  packet[idx++] = (uint8_t)remaining_len;
  packet[idx++] = (uint8_t)(topic_len >> 8);
  packet[idx++] = (uint8_t)(topic_len & 0xFFU);
  (void)memcpy(&packet[idx], topic, topic_len);
  idx = (u16_t)(idx + topic_len);
  (void)memcpy(&packet[idx], payload, payload_len);
  idx = (u16_t)(idx + payload_len);

  write_err = tcp_write(MqttPcb, packet, idx, TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK)
  {
    (void)tcp_output(MqttPcb);
    printf("[MQTT] tx PUBLISH topic=%s payload=%s\r\n", topic, payload);
  }
  else
  {
    printf("[MQTT] write PUBLISH failed topic=%s err=%ld\r\n", topic, (long)write_err);
    MQTT_Client_EnterReconnect("publish write failed");
  }

  return write_err;
}

/**
  * @brief 发布命令执行后的 LED 状态。
  * @param cmd 刚刚执行的命令字符串，例如 led toggle；允许为 NULL。
  * @note
  * - MQTTX 下发 led on/off/toggle 后，STM32 会立即调用本函数回传状态。
  * - 回传 topic 为 stm32/status，payload 是 JSON，包含 led0、led1 和 cmd。
  * - 这样 PC 端不必等下一次 10 秒周期上报，也能马上知道命令执行结果。
  */
static void MQTT_Client_SendStatus(const char *cmd)
{
  char payload[MQTT_PAYLOAD_MAX_LEN]; /* 保存状态回传 JSON。 */

  if (cmd == NULL)
  {
    cmd = "";
  }

  (void)snprintf(payload,
                 sizeof(payload),
                 "{\"led0\":%u,\"led1\":%u,\"cmd\":\"%s\"}",
                 (unsigned int)LED_GetState(LED0),
                 (unsigned int)LED_GetState(LED1),
                 cmd);
  (void)MQTT_Client_SendTextPublish(MqttStatusTopic, payload);
}

/**
  * @brief 关闭当前 MQTT TCP 连接并释放 pcb。
  * @note
  * - 重连前必须清理旧连接，否则旧回调和旧 pcb 可能影响下一次连接。
  * - tcp_close() 失败时使用 tcp_abort()，保证资源不会悬空。
  */
static void MQTT_Client_Close(void)
{
  err_t close_err;

  if (MqttPcb == NULL)
  {
    return;
  }

  tcp_arg(MqttPcb, NULL);
  tcp_recv(MqttPcb, NULL);
  tcp_err(MqttPcb, NULL);

  close_err = tcp_close(MqttPcb);
  if (close_err != ERR_OK)
  {
    tcp_abort(MqttPcb);
  }

  MqttPcb = NULL;
}

/**
  * @brief 统一进入重连等待状态。
  * @param reason 进入重连的原因，用于串口日志；允许为 NULL。
  * @note
  * - 所有错误路径都走这里，避免某些错误只打印日志但不恢复。
  * - 进入该状态后，MQTT_Client_Process() 会等待 MQTT_RECONNECT_DELAY_MS 再重新连接。
  */
static void MQTT_Client_EnterReconnect(const char *reason)
{
  if (reason == NULL)
  {
    reason = "unknown";
  }

  MQTT_Client_Close();
  MqttState = MQTT_CLIENT_STATE_RECONNECT_WAIT;
  MqttReconnectTick = HAL_GetTick();
  MqttStateTick = MqttReconnectTick;
  printf("[MQTT] reconnect wait: %s\r\n", reason);
}

static void MQTT_PrintHex(const uint8_t *data, u16_t len)
{
  u16_t i;

  for (i = 0; i < len; i++)
  {
    printf("%02X", data[i]);
    if ((i + 1U) < len)
    {
      printf(" ");
    }
  }
  printf("\r\n");
}

/**
  * @brief 向 EMQX 发送 SUBSCRIBE，订阅 PC/MQTTX 下发命令的 topic。
  * @note
  * - 本实验只订阅一个 topic：stm32/cmd。
  * - SUBSCRIBE 报文必须带 Packet Identifier，这里固定使用 0x0001。
  * - 发送成功后进入 MQTT_CLIENT_STATE_WAIT_SUBACK，等待 broker 返回 SUBACK。
  * - 收到 SUBACK 后才认为“命令通道”真正建立完成。
  */
static void MQTT_Client_SendSubscribe(void)
{
  err_t write_err;

  if ((MqttPcb == NULL) || (MqttState != MQTT_CLIENT_STATE_WAIT_CONNACK))
  {
    return;
  }

  write_err = tcp_write(MqttPcb,
                        MqttSubscribePacket,
                        (u16_t)sizeof(MqttSubscribePacket),
                        TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK)
  {
    MqttState = MQTT_CLIENT_STATE_WAIT_SUBACK;
    MqttStateTick = HAL_GetTick();
    (void)tcp_output(MqttPcb);
    printf("[MQTT] tx SUBSCRIBE topic=%s: ", MqttCommandTopic);
    MQTT_PrintHex(MqttSubscribePacket, (u16_t)sizeof(MqttSubscribePacket));
  }
  else
  {
    printf("[MQTT] write SUBSCRIBE failed: err=%ld\r\n", (long)write_err);
    MQTT_Client_EnterReconnect("subscribe write failed");
  }
}

/**
  * @brief 执行从 stm32/cmd topic 收到的文本命令。
  * @param cmd MQTT payload 中解析出来的命令字符串。
  * @note
  * - 支持 led on、led off、led toggle 三条命令。
  * - 命令执行后会立即发布 stm32/status，方便 MQTTX 看到执行结果。
  * - 未识别命令只打印日志，不改变 LED 状态。
  */
static void MQTT_Client_HandleCommand(const char *cmd)
{
  if (cmd == NULL)
  {
    return;
  }

  if (strcmp(cmd, "led on") == 0)
  {
    LED_On(LED0);
    printf("[MQTT] cmd led on -> LED0 on\r\n");
    MQTT_Client_SendStatus("led on");
  }
  else if (strcmp(cmd, "led off") == 0)
  {
    LED_Off(LED0);
    printf("[MQTT] cmd led off -> LED0 off\r\n");
    MQTT_Client_SendStatus("led off");
  }
  else if (strcmp(cmd, "led toggle") == 0)
  {
    LED_Toggle(LED0);
    printf("[MQTT] cmd led toggle -> LED0 toggle\r\n");
    MQTT_Client_SendStatus("led toggle");
  }
  else
  {
    printf("[MQTT] cmd unknown: %s\r\n", cmd);
  }
}

/**
  * @brief 解析 broker 发来的 QoS0 PUBLISH 报文。
  * @param data TCP 接收缓冲中的 MQTT 原始报文字节。
  * @param len  data 的有效长度。
  * @note
  * - 当前学习阶段只处理 Remaining Length 为单字节的短 PUBLISH。
  * - 解析顺序：固定报头 -> Remaining Length -> Topic Length -> Topic -> Payload。
  * - 如果 topic 等于 stm32/cmd，就把 payload 当作命令交给 MQTT_Client_HandleCommand()。
  */
static void MQTT_Client_HandlePublish(const uint8_t *data, u16_t len)
{
  u16_t remaining_len;
  u16_t topic_len;
  u16_t topic_start;
  u16_t payload_start;
  u16_t payload_len;
  char topic[32];
  char payload[MQTT_CMD_MAX_LEN];

  if ((data == NULL) || (len < 5U) || ((data[0] & 0xF0U) != 0x30U))
  {
    return;
  }

  if ((data[1] & 0x80U) != 0U)
  {
    printf("[MQTT] PUBLISH remaining length too large\r\n");
    return;
  }

  remaining_len = data[1];
  if ((remaining_len + 2U) > len)
  {
    printf("[MQTT] PUBLISH packet incomplete\r\n");
    return;
  }

  topic_start = 4U;
  topic_len = (u16_t)(((u16_t)data[2] << 8) | data[3]);
  payload_start = (u16_t)(topic_start + topic_len);
  if ((topic_len >= sizeof(topic)) || (payload_start > len))
  {
    printf("[MQTT] PUBLISH topic too large\r\n");
    return;
  }

  payload_len = (u16_t)(len - payload_start);
  if (payload_len >= sizeof(payload))
  {
    payload_len = (u16_t)(sizeof(payload) - 1U);
  }

  (void)memcpy(topic, &data[topic_start], topic_len);
  topic[topic_len] = '\0';
  (void)memcpy(payload, &data[payload_start], payload_len);
  payload[payload_len] = '\0';

  while ((payload_len > 0U) &&
         ((payload[payload_len - 1U] == '\r') || (payload[payload_len - 1U] == '\n')))
  {
    payload_len--;
    payload[payload_len] = '\0';
  }

  printf("[MQTT] rx PUBLISH topic=%s payload=%s\r\n", topic, payload);
  if (strcmp(topic, MqttCommandTopic) == 0)
  {
    MQTT_Client_HandleCommand(payload);
  }
}

static void MQTT_Client_SendPingReq(void)
{
  err_t write_err;

  if ((MqttPcb == NULL) || (MqttState != MQTT_CLIENT_STATE_MQTT_CONNECTED))
  {
    return;
  }

  if ((HAL_GetTick() - MqttLastPingTick) < MQTT_PING_INTERVAL_MS)
  {
    return;
  }

  MqttLastPingTick = HAL_GetTick();
  write_err = tcp_write(MqttPcb, MqttPingReqPacket, (u16_t)sizeof(MqttPingReqPacket), TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK)
  {
    (void)tcp_output(MqttPcb);
    printf("[MQTT] tx PINGREQ: ");
    MQTT_PrintHex(MqttPingReqPacket, (u16_t)sizeof(MqttPingReqPacket));
  }
  else
  {
    printf("[MQTT] write PINGREQ failed: err=%ld\r\n", (long)write_err);
    MQTT_Client_EnterReconnect("ping write failed");
  }
}

/**
  * @brief 周期发布 STM32 的传感器数据和 LED 状态。
  * @note
  * - 发布 topic 为 stm32/sensor。
  * - 函数每 MQTT_PUBLISH_INTERVAL_MS 执行一次，由 MQTT_Client_Process() 调用。
  * - 传感器值来自 App_GetSensorSnapshot()，该函数读取 main.c 中的缓存值。
  * - 上报 JSON 同时带 led0/led1，方便 MQTTX 看到当前输出状态。
  */
static void MQTT_Client_SendPublish(void)
{
  App_SensorSnapshot_t sensor;
  char payload[MQTT_PAYLOAD_MAX_LEN]; /* 保存周期上报 JSON。 */

  if ((MqttPcb == NULL) || (MqttState != MQTT_CLIENT_STATE_MQTT_CONNECTED))
  {
    return;
  }

  if ((HAL_GetTick() - MqttLastPublishTick) < MQTT_PUBLISH_INTERVAL_MS)
  {
    return;
  }

  /* 从 main.c 的缓存中取值，避免在 MQTT 发送路径里阻塞读取传感器。 */
  App_GetSensorSnapshot(&sensor);
  (void)snprintf(payload,
                 sizeof(payload),
                 "{\"temp\":%.1f,\"humi\":%.1f,\"light_adc\":%u,\"light_v\":%.3f,\"led0\":%u,\"led1\":%u}",
                 sensor.temperature,
                 sensor.humidity,
                 (unsigned int)sensor.light_adc,
                 sensor.light_voltage,
                 (unsigned int)LED_GetState(LED0),
                 (unsigned int)LED_GetState(LED1));

  MqttLastPublishTick = HAL_GetTick();
  (void)MQTT_Client_SendTextPublish(MqttSensorTopic, payload);
}

/**
  * @brief 检查网口 link 状态。
  * @note
  * - link down 时不能继续写 TCP，否则只是不断产生错误。
  * - link 重新 up 后，状态机从 IDLE/RECONNECT_WAIT 重新开始连接 Broker。
  */
static void MQTT_Client_CheckLink(void)
{
  uint8_t link_up = (netif_is_link_up(&gnetif) != 0) ? 1U : 0U;

  if (link_up == 0U)
  {
    if ((MqttLinkWasUp != 0U) && (MqttState != MQTT_CLIENT_STATE_IDLE))
    {
      printf("[MQTT] link down, close connection\r\n");
      MQTT_Client_Close();
      MqttState = MQTT_CLIENT_STATE_IDLE;
      MqttStartTick = HAL_GetTick();
      MqttStateTick = MqttStartTick;
    }
    MqttLinkWasUp = 0U;
    return;
  }

  if (MqttLinkWasUp == 0U)
  {
    printf("[MQTT] link up, reconnect allowed\r\n");
    MqttStartTick = HAL_GetTick();
    MqttStateTick = MqttStartTick;
  }
  MqttLinkWasUp = 1U;
}

/**
  * @brief 检查 MQTT 握手阶段是否超时。
  * @note
  * - TCP 连上但迟迟没有 CONNACK，说明 MQTT 层没有建立成功。
  * - SUBSCRIBE 发出但迟迟没有 SUBACK，说明命令通道没有建立成功。
  */
static void MQTT_Client_CheckTimeout(void)
{
  uint32_t now = HAL_GetTick();

  if ((MqttState == MQTT_CLIENT_STATE_WAIT_CONNACK) &&
      ((now - MqttStateTick) >= MQTT_CONNACK_TIMEOUT_MS))
  {
    MQTT_Client_EnterReconnect("CONNACK timeout");
  }
  else if ((MqttState == MQTT_CLIENT_STATE_WAIT_SUBACK) &&
           ((now - MqttStateTick) >= MQTT_SUBACK_TIMEOUT_MS))
  {
    MQTT_Client_EnterReconnect("SUBACK timeout");
  }
}

static void MQTT_Client_StartConnect(void)
{
  ip_addr_t broker_ip;
  err_t err;

  if (!netif_is_link_up(&gnetif))
  {
    return;
  }

  if ((HAL_GetTick() - MqttStartTick) < MQTT_CONNECT_DELAY_MS)
  {
    return;
  }

  MQTT_Client_Close();
  MqttPcb = tcp_new();
  if (MqttPcb == NULL)
  {
    MQTT_Client_EnterReconnect("create pcb failed");
    return;
  }

  tcp_arg(MqttPcb, NULL);
  tcp_recv(MqttPcb, MQTT_Client_Recv);
  tcp_err(MqttPcb, MQTT_Client_Error);

  IP4_ADDR(ip_2_ip4(&broker_ip),
           MQTT_BROKER_IP0,
           MQTT_BROKER_IP1,
           MQTT_BROKER_IP2,
           MQTT_BROKER_IP3);

  MqttState = MQTT_CLIENT_STATE_TCP_CONNECTING;
  MqttStateTick = HAL_GetTick();
  printf("[MQTT] tcp connect to %u.%u.%u.%u:%u\r\n",
         (unsigned int)MQTT_BROKER_IP0,
         (unsigned int)MQTT_BROKER_IP1,
         (unsigned int)MQTT_BROKER_IP2,
         (unsigned int)MQTT_BROKER_IP3,
         (unsigned int)MQTT_BROKER_PORT);

  err = tcp_connect(MqttPcb, &broker_ip, MQTT_BROKER_PORT, MQTT_Client_Connected);
  if (err != ERR_OK)
  {
    printf("[MQTT] tcp_connect failed: err=%ld\r\n", (long)err);
    MQTT_Client_EnterReconnect("tcp_connect failed");
  }
}

static err_t MQTT_Client_Connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
  err_t write_err;

  LWIP_UNUSED_ARG(arg);

  if ((err != ERR_OK) || (tpcb == NULL))
  {
    printf("[MQTT] tcp connected callback err=%ld\r\n", (long)err);
    MQTT_Client_EnterReconnect("tcp connected callback failed");
    return err;
  }

  MqttPcb = tpcb;
  printf("[MQTT] tcp connected\r\n");

  write_err = tcp_write(tpcb, MqttConnectPacket, (u16_t)sizeof(MqttConnectPacket), TCP_WRITE_FLAG_COPY);
  if (write_err == ERR_OK)
  {
    MqttState = MQTT_CLIENT_STATE_WAIT_CONNACK;
    MqttStateTick = HAL_GetTick();
    (void)tcp_output(tpcb);
    printf("[MQTT] tx CONNECT: ");
    MQTT_PrintHex(MqttConnectPacket, (u16_t)sizeof(MqttConnectPacket));
    return ERR_OK;
  }

  printf("[MQTT] write CONNECT failed: err=%ld\r\n", (long)write_err);
  MQTT_Client_EnterReconnect("CONNECT write failed");
  return write_err;
}

static err_t MQTT_Client_Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  uint8_t rx[MQTT_RX_MAX_LEN];
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
    printf("[MQTT] broker closed\r\n");
    MQTT_Client_EnterReconnect("broker closed");
    return ERR_OK;
  }

  if (err != ERR_OK)
  {
    pbuf_free(p);
    MQTT_Client_EnterReconnect("recv error");
    return err;
  }

  copy_len = p->tot_len;
  if (copy_len > sizeof(rx))
  {
    copy_len = (u16_t)sizeof(rx);
  }

  (void)pbuf_copy_partial(p, rx, copy_len, 0U);
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  printf("[MQTT] rx: ");
  MQTT_PrintHex(rx, copy_len);

  if ((copy_len >= 4U) && (rx[0] == 0x20U) && (rx[1] == 0x02U) && (rx[3] == 0x00U))
  {
    MqttState = MQTT_CLIENT_STATE_WAIT_CONNACK;
    MqttLastPingTick = HAL_GetTick();
    MqttLastPublishTick = HAL_GetTick();
    printf("[MQTT] rx CONNACK accepted\r\n");
    MQTT_Client_SendSubscribe();
  }
  else if ((copy_len >= 5U) && (rx[0] == 0x90U) && (rx[1] == 0x03U) && (rx[4] == 0x00U))
  {
    MqttState = MQTT_CLIENT_STATE_MQTT_CONNECTED;
    MqttStateTick = HAL_GetTick();
    MqttLastPingTick = HAL_GetTick();
    MqttLastPublishTick = HAL_GetTick();
    printf("[MQTT] rx SUBACK accepted topic=%s\r\n", MqttCommandTopic);
  }
  else if ((copy_len >= 2U) && (rx[0] == 0xD0U) && (rx[1] == 0x00U))
  {
    printf("[MQTT] rx PINGRESP\r\n");
  }
  else if ((copy_len >= 2U) && ((rx[0] & 0xF0U) == 0x30U))
  {
    MQTT_Client_HandlePublish(rx, copy_len);
  }
  else
  {
    printf("[MQTT] unexpected packet\r\n");
  }

  return ERR_OK;
}

static void MQTT_Client_Error(void *arg, err_t err)
{
  LWIP_UNUSED_ARG(arg);
  MqttPcb = NULL;
  MqttState = MQTT_CLIENT_STATE_RECONNECT_WAIT;
  MqttReconnectTick = HAL_GetTick();
  MqttStateTick = MqttReconnectTick;
  printf("[MQTT] error: err=%ld, reconnect wait\r\n", (long)err);
}

void MQTT_Client_Init(void)
{
  MqttStarted = 1U;
  MqttLinkWasUp = 0U;
  MqttStartTick = HAL_GetTick();
  MqttStateTick = HAL_GetTick();
  MqttReconnectTick = HAL_GetTick();
  MqttLastPingTick = HAL_GetTick();
  MqttLastPublishTick = HAL_GetTick();
  MqttState = MQTT_CLIENT_STATE_IDLE;
  printf("[MQTT] client ready\r\n");
}

void MQTT_Client_Process(void)
{
  if (MqttStarted == 0U)
  {
    return;
  }

  MQTT_Client_CheckLink();
  MQTT_Client_CheckTimeout();

  if (MqttState == MQTT_CLIENT_STATE_IDLE)
  {
    MQTT_Client_StartConnect();
  }
  else if (MqttState == MQTT_CLIENT_STATE_RECONNECT_WAIT)
  {
    if ((HAL_GetTick() - MqttReconnectTick) >= MQTT_RECONNECT_DELAY_MS)
    {
      MqttState = MQTT_CLIENT_STATE_IDLE;
      MqttStartTick = HAL_GetTick() - MQTT_CONNECT_DELAY_MS;
    }
  }
  else if (MqttState == MQTT_CLIENT_STATE_ERROR)
  {
    MQTT_Client_EnterReconnect("state error");
  }
  else if (MqttState == MQTT_CLIENT_STATE_MQTT_CONNECTED)
  {
    MQTT_Client_SendPublish();
    MQTT_Client_SendPingReq();
  }
}