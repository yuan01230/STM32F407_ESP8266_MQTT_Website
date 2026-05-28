#include "aliyun_iot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "aliyun_iot_config.h"
#include "../delay/delay.h"
#include "../led/led.h"

/* ========================= ????? ========================= */

/*
 * ESP8266 ??? MQTT ?????
 *
 * ???
 * 1. BOOT ???????
 * 2. AT ~ MQTTSUB ????????
 * 3. MQTTPUB ???????
 * 4. DONE ??????????
 * 5. ERROR ?????????????????????
 */
typedef enum
{
  ESP8266_STEP_BOOT = 0,
  ESP8266_STEP_AT,
  ESP8266_STEP_ATE0,
  ESP8266_STEP_CWMODE,
  ESP8266_STEP_CWJAP,
  ESP8266_STEP_MQTTUSERCFG,
  ESP8266_STEP_MQTTCLIENTID,
  ESP8266_STEP_MQTTUSERNAME,
  ESP8266_STEP_MQTTPASSWORD,
  ESP8266_STEP_MQTTCONN,
  ESP8266_STEP_MQTTSUB,
  ESP8266_STEP_MQTTPUB,
  ESP8266_STEP_DONE,
  ESP8266_STEP_ERROR
} esp8266_iot_step_t;

/*
 * ??????
 *
 * ???????????
 * 1. ??????
 * 2. ??????
 * ???????????????? 10s ?????
 */
typedef enum
{
  ALIYUN_PUBLISH_NONE = 0,
  ALIYUN_PUBLISH_PROPERTY_POST,
  ALIYUN_PUBLISH_SET_REPLY
} aliyun_publish_kind_t;

/*
 * ??????
 *
 * ????? set_reply ???????????
 * ?????????????????
 */
#define ALIYUN_PUBLISH_QUEUE_SIZE 4U

/* ========================= ????? ========================= */

/* ? ESP8266 ?????????????? USART3 */
static UART_HandleTypeDef *g_aliyun_uart = NULL;
static AliyunIoT_SensorData_t g_sensor_data = {0};
static uint8_t g_esp8266_rx_byte = 0U;
static char g_esp8266_rx_line[256] = "";
static uint16_t g_esp8266_rx_index = 0U;
static char g_esp8266_last_line[256] = "Booting";
static uint8_t g_esp8266_ready = 0U;
static uint32_t g_esp8266_ok_count = 0U;
static uint32_t g_esp8266_rx_count = 0U;
static uint8_t g_esp8266_waiting_ok = 0U;
static uint8_t g_esp8266_rsp_ok = 0U;
static uint8_t g_esp8266_rsp_error = 0U;
static uint8_t g_esp8266_iot_enabled = 1U;
static uint8_t g_esp8266_publish_done = 0U;
static uint8_t g_esp8266_retry_count = 0U;
static uint8_t g_esp8266_rsp_prompt = 0U;
static uint8_t g_esp8266_waiting_prompt = 0U;
static uint32_t g_esp8266_cmd_deadline = 0U;
static esp8266_iot_step_t g_esp8266_step = ESP8266_STEP_BOOT;
static char g_esp8266_cmd_buf[256] = "";
/* ??????? MQTT topic ? payload */
static char g_mqtt_pub_topic[128] = "";
static char g_mqtt_pub_payload[384] = "";
static char g_aliyun_prop_payload[256] = "";
/* 10s ????????? */
static delay_nb_timer_t g_aliyun_post_timer;
/* ??????????? */
static aliyun_publish_kind_t g_publish_kind = ALIYUN_PUBLISH_NONE;

/* ========================= ????? ========================= */
static char g_publish_queue_topic[ALIYUN_PUBLISH_QUEUE_SIZE][128];
static char g_publish_queue_payload[ALIYUN_PUBLISH_QUEUE_SIZE][384];
static aliyun_publish_kind_t g_publish_queue_kind[ALIYUN_PUBLISH_QUEUE_SIZE];
static uint8_t g_publish_queue_head = 0U;
static uint8_t g_publish_queue_tail = 0U;
static uint8_t g_publish_queue_count = 0U;

/* ========================= ??????? ========================= */

static void ESP8266_StartReceiveIT(void);
static void ESP8266_SendString(const char *text);
static uint8_t ESP8266_BuildCommand(esp8266_iot_step_t step, char *buf, size_t buf_size, uint32_t *timeout_ms);
static void ESP8266_SendStep(esp8266_iot_step_t step);
static void Aliyun_BuildPropertyPayload(void);
static void Aliyun_EnqueuePublish(aliyun_publish_kind_t kind, const char *topic, const char *payload);
static uint8_t Aliyun_DequeuePublish(void);
static void Aliyun_TryStartNextPublish(void);
static void Aliyun_RequestSetReply(const char *msg_id);
static void Aliyun_HandlePropertySetMessage(const char *line);

/* ========================= AT ????? ========================= */

/* ?? 1 ???????????????? */
static void ESP8266_StartReceiveIT(void)
{
  if (g_aliyun_uart != NULL)
  {
    (void)HAL_UART_Receive_IT(g_aliyun_uart, &g_esp8266_rx_byte, 1U);
  }
}

/* ? ESP8266 ???????? payload */
static void ESP8266_SendString(const char *text)
{
  if ((text == NULL) || (g_aliyun_uart == NULL))
  {
    return;
  }

  (void)HAL_UART_Transmit(g_aliyun_uart, (uint8_t *)text, (uint16_t)strlen(text), 1000U);
}

/*
 * ???????? AT ????????????
 */
static uint8_t ESP8266_BuildCommand(esp8266_iot_step_t step, char *buf, size_t buf_size, uint32_t *timeout_ms)
{
  int len = 0;

  if ((buf == NULL) || (buf_size == 0U) || (timeout_ms == NULL))
  {
    return 0U;
  }

  *timeout_ms = 3000U;
  switch (step)
  {
    case ESP8266_STEP_AT:
      len = snprintf(buf, buf_size, "AT\r\n");
      *timeout_ms = 2000U;
      break;

    case ESP8266_STEP_ATE0:
      len = snprintf(buf, buf_size, "ATE0\r\n");
      *timeout_ms = 2000U;
      break;

    case ESP8266_STEP_CWMODE:
      len = snprintf(buf, buf_size, "AT+CWMODE=1\r\n");
      *timeout_ms = 3000U;
      break;

    case ESP8266_STEP_CWJAP:
      len = snprintf(buf, buf_size, "AT+CWJAP=\"%s\",\"%s\"\r\n", ALIYUN_IOT_WIFI_SSID, ALIYUN_IOT_WIFI_PASSWORD);
      *timeout_ms = 30000U;
      break;

    case ESP8266_STEP_MQTTUSERCFG:
      len = snprintf(buf, buf_size, "AT+MQTTUSERCFG=0,1,\"\",\"\",\"\",0,0,\"\"\r\n");
      *timeout_ms = 5000U;
      break;

    case ESP8266_STEP_MQTTCLIENTID:
      len = snprintf(buf, buf_size, "AT+MQTTCLIENTID=0,\"%s\"\r\n", ALIYUN_IOT_MQTT_CLIENT_ID_AT);
      *timeout_ms = 5000U;
      break;

    case ESP8266_STEP_MQTTUSERNAME:
      len = snprintf(buf, buf_size, "AT+MQTTUSERNAME=0,\"%s\"\r\n", ALIYUN_IOT_MQTT_USERNAME);
      *timeout_ms = 5000U;
      break;

    case ESP8266_STEP_MQTTPASSWORD:
      len = snprintf(buf, buf_size, "AT+MQTTPASSWORD=0,\"%s\"\r\n", ALIYUN_IOT_MQTT_PASSWORD);
      *timeout_ms = 5000U;
      break;

    case ESP8266_STEP_MQTTCONN:
      len = snprintf(buf, buf_size, "AT+MQTTCONN=0,\"%s\",%u,0\r\n", ALIYUN_IOT_MQTT_HOST, (unsigned int)ALIYUN_IOT_MQTT_PORT);
      *timeout_ms = 30000U;
      break;

    case ESP8266_STEP_MQTTSUB:
      len = snprintf(buf, buf_size, "AT+MQTTSUB=0,\"%s\",0\r\n", ALIYUN_IOT_PROP_SET_TOPIC);
      *timeout_ms = 10000U;
      break;

    case ESP8266_STEP_MQTTPUB:
      if ((g_mqtt_pub_topic[0] == '\0') || (g_mqtt_pub_payload[0] == '\0'))
      {
        return 0U;
      }
      len = snprintf(buf,
                     buf_size,
                     "AT+MQTTPUBRAW=0,\"%s\",%u,0,0\r\n",
                     g_mqtt_pub_topic,
                     (unsigned int)strlen(g_mqtt_pub_payload));
      *timeout_ms = 10000U;
      break;

    default:
      return 0U;
  }

  if ((len <= 0) || ((size_t)len >= buf_size))
  {
    return 0U;
  }

  return 1U;
}

/*
 * ??????????? AT ??
 * ??????????????????
 */
static void ESP8266_SendStep(esp8266_iot_step_t step)
{
  uint32_t timeout_ms = 0U;

  if (ESP8266_BuildCommand(step, g_esp8266_cmd_buf, sizeof(g_esp8266_cmd_buf), &timeout_ms) == 0U)
  {
    strncpy(g_esp8266_last_line, "Config too long", sizeof(g_esp8266_last_line) - 1U);
    g_esp8266_last_line[sizeof(g_esp8266_last_line) - 1U] = '\0';
    g_esp8266_step = ESP8266_STEP_ERROR;
    return;
  }

  g_esp8266_rsp_ok = 0U;
  g_esp8266_rsp_error = 0U;
  g_esp8266_rsp_prompt = 0U;
  g_esp8266_waiting_ok = 1U;
  g_esp8266_waiting_prompt = 0U;

  if (step == ESP8266_STEP_MQTTPUB)
  {
    g_esp8266_waiting_ok = 0U;
    g_esp8266_waiting_prompt = 1U;
  }

  g_esp8266_cmd_deadline = HAL_GetTick() + timeout_ms;
  ESP8266_SendString(g_esp8266_cmd_buf);
  printf("[ESP8266] TX: %s", g_esp8266_cmd_buf);
}

/* ========================= ?????????? ========================= */

/* ?????? JSON????? main.c ?????????? */
static void Aliyun_BuildPropertyPayload(void)
{
  int led_switch0 = (HAL_GPIO_ReadPin(LED0_GPIO_Port, LED0_Pin) == GPIO_PIN_RESET) ? 1 : 0;
  int led_switch1 = (HAL_GPIO_ReadPin(LED1_GPIO_Port, LED1_Pin) == GPIO_PIN_RESET) ? 1 : 0;

  (void)snprintf(g_aliyun_prop_payload,
                 sizeof(g_aliyun_prop_payload),
                 "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"temperature\":%.1f,\"Humidity\":%.1f,\"LEDSwitch0\":%d,\"LEDSwitch1\":%d,\"Light_Out_Voltage\":%.2f,\"Yaw\":%.2f,\"Roll\":%.2f,\"Pitch\":%.2f,\"Light_ADC\":%u},\"method\":\"thing.event.property.post\"}",
                 (unsigned long)HAL_GetTick(),
                 (double)g_sensor_data.temperature,
                 (double)g_sensor_data.humidity,
                 led_switch0,
                 led_switch1,
                 (double)g_sensor_data.light_voltage,
                 (double)g_sensor_data.yaw,
                 (double)g_sensor_data.roll,
                 (double)g_sensor_data.pitch,
                 (unsigned int)g_sensor_data.light_adc);
}

/*
 * ???????????
 *
 * ???
 * ?????????????????????
 * ? Aliyun_TryStartNextPublish() ????
 */
static void Aliyun_EnqueuePublish(aliyun_publish_kind_t kind, const char *topic, const char *payload)
{
  uint8_t slot = 0U;

  if ((topic == NULL) || (payload == NULL) || (topic[0] == '\0') || (payload[0] == '\0'))
  {
    return;
  }

  if (g_publish_queue_count >= ALIYUN_PUBLISH_QUEUE_SIZE)
  {
    g_publish_queue_head = (uint8_t)((g_publish_queue_head + 1U) % ALIYUN_PUBLISH_QUEUE_SIZE);
    g_publish_queue_count--;
    printf("[Aliyun] publish queue full, drop oldest\r\n");
  }

  slot = g_publish_queue_tail;
  strncpy(g_publish_queue_topic[slot], topic, sizeof(g_publish_queue_topic[slot]) - 1U);
  g_publish_queue_topic[slot][sizeof(g_publish_queue_topic[slot]) - 1U] = '\0';
  strncpy(g_publish_queue_payload[slot], payload, sizeof(g_publish_queue_payload[slot]) - 1U);
  g_publish_queue_payload[slot][sizeof(g_publish_queue_payload[slot]) - 1U] = '\0';
  g_publish_queue_kind[slot] = kind;
  g_publish_queue_tail = (uint8_t)((g_publish_queue_tail + 1U) % ALIYUN_PUBLISH_QUEUE_SIZE);
  g_publish_queue_count++;
}

/* ???????????????? */
static uint8_t Aliyun_DequeuePublish(void)
{
  uint8_t slot = 0U;

  if (g_publish_queue_count == 0U)
  {
    return 0U;
  }

  slot = g_publish_queue_head;
  strncpy(g_mqtt_pub_topic, g_publish_queue_topic[slot], sizeof(g_mqtt_pub_topic) - 1U);
  g_mqtt_pub_topic[sizeof(g_mqtt_pub_topic) - 1U] = '\0';
  strncpy(g_mqtt_pub_payload, g_publish_queue_payload[slot], sizeof(g_mqtt_pub_payload) - 1U);
  g_mqtt_pub_payload[sizeof(g_mqtt_pub_payload) - 1U] = '\0';
  g_publish_kind = g_publish_queue_kind[slot];
  g_publish_queue_head = (uint8_t)((g_publish_queue_head + 1U) % ALIYUN_PUBLISH_QUEUE_SIZE);
  g_publish_queue_count--;
  return 1U;
}

/*
 * ???? ESP8266 ??? DONE??????????
 * ????????? MQTT ??
 */
static void Aliyun_TryStartNextPublish(void)
{
  if (g_esp8266_step != ESP8266_STEP_DONE)
  {
    return;
  }

  if ((g_esp8266_waiting_ok != 0U) || (g_esp8266_waiting_prompt != 0U))
  {
    return;
  }

  if (Aliyun_DequeuePublish() == 0U)
  {
    return;
  }

  g_esp8266_publish_done = 0U;
  g_esp8266_retry_count = 0U;
  g_esp8266_step = ESP8266_STEP_MQTTPUB;
  ESP8266_SendStep(g_esp8266_step);
}

/* ??????????????????????????? */
void AliyunIoT_RequestPropertyPost(void)
{
  Aliyun_BuildPropertyPayload();
  Aliyun_EnqueuePublish(ALIYUN_PUBLISH_PROPERTY_POST, ALIYUN_IOT_PROP_POST_TOPIC, g_aliyun_prop_payload);
}

/*
 * ??? property/set ??????
 *
 * ?????????????????????????????
 */
static void Aliyun_RequestSetReply(const char *msg_id)
{
  int led_switch0 = (HAL_GPIO_ReadPin(LED0_GPIO_Port, LED0_Pin) == GPIO_PIN_RESET) ? 1 : 0;
  int led_switch1 = (HAL_GPIO_ReadPin(LED1_GPIO_Port, LED1_Pin) == GPIO_PIN_RESET) ? 1 : 0;
  char reply_payload[128];

  if (msg_id == NULL)
  {
    msg_id = "0";
  }

  (void)snprintf(reply_payload,
                 sizeof(reply_payload),
                 "{\"id\":\"%s\",\"code\":200,\"data\":{\"LEDSwitch0\":%d,\"LEDSwitch1\":%d}}",
                 msg_id,
                 led_switch0,
                 led_switch1);
  Aliyun_EnqueuePublish(ALIYUN_PUBLISH_SET_REPLY, ALIYUN_IOT_PROP_SET_REPLY_TOPIC, reply_payload);
}

/*
 * ???? property/set ??
 *
 * ??????
 * - LEDSwitch0
 * - LEDSwitch1
 */
static void Aliyun_HandlePropertySetMessage(const char *line)
{
  const char *led0_pos;
  const char *led1_pos;
  const char *id_pos;
  char msg_id[24] = "0";
  int led0_value = -1;
  int led1_value = -1;

  if (line == NULL)
  {
    return;
  }

  led0_pos = strstr(line, "\"LEDSwitch0\":");
  if (led0_pos != NULL)
  {
    led0_pos += strlen("\"LEDSwitch0\":");
    led0_value = atoi(led0_pos);
  }

  led1_pos = strstr(line, "\"LEDSwitch1\":");
  if (led1_pos != NULL)
  {
    led1_pos += strlen("\"LEDSwitch1\":");
    led1_value = atoi(led1_pos);
  }

  id_pos = strstr(line, "\"id\":\"");
  if (id_pos != NULL)
  {
    size_t i = 0U;
    id_pos += strlen("\"id\":\"");
    while ((id_pos[i] != '\0') && (id_pos[i] != '"') && (i < (sizeof(msg_id) - 1U)))
    {
      msg_id[i] = id_pos[i];
      i++;
    }
    msg_id[i] = '\0';
  }

  if (led0_value >= 0)
  {
    if (led0_value != 0)
    {
      LED_On(LED0);
    }
    else
    {
      LED_Off(LED0);
    }
    printf("[Aliyun] LEDSwitch0=%d\r\n", led0_value);
  }

  if (led1_value >= 0)
  {
    if (led1_value != 0)
    {
      LED_On(LED1);
    }
    else
    {
      LED_Off(LED1);
    }
    printf("[Aliyun] LEDSwitch1=%d\r\n", led1_value);
  }

  if ((led0_value >= 0) || (led1_value >= 0))
  {
    Aliyun_RequestSetReply(msg_id);
  }
}

/* ========================= ?????? ========================= */

/*
 * ???????
 *
 * ???
 * 1. ?? ESP8266
 * 2. ????????
 * 3. ??????
 * 4. ????????
 */
void AliyunIoT_Init(UART_HandleTypeDef *huart)
{
  g_aliyun_uart = huart;
  HAL_GPIO_WritePin(ESP8266EN_GPIO_Port, ESP8266EN_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(ESP8266EST_GPIO_Port, ESP8266EST_Pin, GPIO_PIN_RESET);
  HAL_Delay(50U);
  HAL_GPIO_WritePin(ESP8266EST_GPIO_Port, ESP8266EST_Pin, GPIO_PIN_SET);
  HAL_Delay(500U);

  memset(&g_sensor_data, 0, sizeof(g_sensor_data));
  g_esp8266_rx_byte = 0U;
  g_esp8266_rx_line[0] = '\0';
  g_esp8266_rx_index = 0U;
  strncpy(g_esp8266_last_line, "Reset done", sizeof(g_esp8266_last_line) - 1U);
  g_esp8266_last_line[sizeof(g_esp8266_last_line) - 1U] = '\0';
  g_esp8266_ready = 0U;
  g_esp8266_ok_count = 0U;
  g_esp8266_rx_count = 0U;
  g_esp8266_waiting_ok = 0U;
  g_esp8266_rsp_ok = 0U;
  g_esp8266_rsp_error = 0U;
  g_esp8266_iot_enabled = 1U;
  g_esp8266_publish_done = 0U;
  g_esp8266_retry_count = 0U;
  g_esp8266_rsp_prompt = 0U;
  g_esp8266_waiting_prompt = 0U;
  g_esp8266_cmd_deadline = 0U;
  g_esp8266_step = ESP8266_STEP_BOOT;
  g_esp8266_cmd_buf[0] = '\0';
  g_mqtt_pub_topic[0] = '\0';
  g_mqtt_pub_payload[0] = '\0';
  g_aliyun_prop_payload[0] = '\0';
  g_publish_kind = ALIYUN_PUBLISH_NONE;
  g_publish_queue_head = 0U;
  g_publish_queue_tail = 0U;
  g_publish_queue_count = 0U;
  ESP8266_StartReceiveIT();
}

/* ========================= ?????? ========================= */

/*
 * ???????
 *
 * ? main ???????????
 * 1. ???????
 * 2. ????? MQTT ??
 * 3. ?? prompt / OK / timeout
 * 4. ????????
 */
void AliyunIoT_Task(void)
{
  if (g_esp8266_iot_enabled == 0U)
  {
    return;
  }

  if (g_esp8266_step == ESP8266_STEP_BOOT)
  {
    g_esp8266_step = ESP8266_STEP_AT;
    ESP8266_SendStep(g_esp8266_step);
    return;
  }

  if (g_esp8266_step == ESP8266_STEP_DONE)
  {
    if (delay_nb_is_expired(&g_aliyun_post_timer) == 1U)
    {
      AliyunIoT_RequestPropertyPost();
    }
    Aliyun_TryStartNextPublish();
    return;
  }

  if (g_esp8266_step == ESP8266_STEP_ERROR)
  {
    return;
  }

  if (g_esp8266_rsp_prompt != 0U)
  {
    g_esp8266_rsp_prompt = 0U;
    g_esp8266_waiting_prompt = 0U;
    g_esp8266_waiting_ok = 1U;
    g_esp8266_cmd_deadline = HAL_GetTick() + 5000U;

    if (g_esp8266_step == ESP8266_STEP_MQTTPUB)
    {
      ESP8266_SendString(g_mqtt_pub_payload);
      printf("[ESP8266] TX RAW: payload\r\n");
    }

    return;
  }

  if (g_esp8266_rsp_ok != 0U)
  {
    g_esp8266_rsp_ok = 0U;
    g_esp8266_retry_count = 0U;
    g_esp8266_waiting_ok = 0U;

    if (g_esp8266_step == ESP8266_STEP_MQTTPUB)
    {
      g_esp8266_publish_done = 1U;
      g_esp8266_step = ESP8266_STEP_DONE;
      strncpy(g_esp8266_last_line, "Aliyun pub ok", sizeof(g_esp8266_last_line) - 1U);
      g_esp8266_last_line[sizeof(g_esp8266_last_line) - 1U] = '\0';
      if (g_publish_kind == ALIYUN_PUBLISH_PROPERTY_POST)
      {
        delay_nb_start_ms(&g_aliyun_post_timer, ALIYUN_IOT_PROP_POST_INTERVAL_MS);
      }
      g_publish_kind = ALIYUN_PUBLISH_NONE;
      return;
    }

    if (g_esp8266_step == ESP8266_STEP_MQTTSUB)
    {
      g_esp8266_step = ESP8266_STEP_DONE;
      AliyunIoT_RequestPropertyPost();
      return;
    }

    g_esp8266_step = (esp8266_iot_step_t)((int)g_esp8266_step + 1);
    ESP8266_SendStep(g_esp8266_step);
    return;
  }

  if ((g_esp8266_rsp_error != 0U) ||
      ((g_esp8266_waiting_ok != 0U) && ((int32_t)(HAL_GetTick() - g_esp8266_cmd_deadline) >= 0)))
  {
    g_esp8266_rsp_error = 0U;
    g_esp8266_waiting_ok = 0U;

    if (g_esp8266_retry_count < 2U)
    {
      g_esp8266_retry_count++;
      ESP8266_SendStep(g_esp8266_step);
    }
    else
    {
      g_esp8266_step = ESP8266_STEP_ERROR;
      strncpy(g_esp8266_last_line, "IoT step failed", sizeof(g_esp8266_last_line) - 1U);
      g_esp8266_last_line[sizeof(g_esp8266_last_line) - 1U] = '\0';
      printf("[ESP8266] step %d failed\r\n", (int)g_esp8266_step);
    }
  }
}

void AliyunIoT_UpdateSensorData(const AliyunIoT_SensorData_t *data)
{
  if (data != NULL)
  {
    g_sensor_data = *data;
  }
}

/* ========================= UART ??????? ========================= */

/*
 * ESP8266 ??????????
 *
 * ?????
 * 1. ????????
 * 2. ?? > ???
 * 3. ?? OK / +MQTTPUB:OK / WIFI GOT IP / +MQTTCONNECTED
 * 4. ?? +MQTTSUBRECV ????????
 */
/* ========================= UART ??????? ========================= */

/*
 * ESP8266 ??????????
 *
 * ?????
 * 1. ????????
 * 2. ?? > ???
 * 3. ?? OK / +MQTTPUB:OK / WIFI GOT IP / +MQTTCONNECTED
 * 4. ?? +MQTTSUBRECV ????????
 */
void AliyunIoT_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((huart != g_aliyun_uart) || (g_aliyun_uart == NULL))
  {
    return;
  }

  g_esp8266_rx_count++;
  if (g_esp8266_rx_byte == '>')
  {
    g_esp8266_waiting_ok = 0U;
    g_esp8266_waiting_prompt = 0U;
    g_esp8266_rsp_prompt = 1U;
    ESP8266_StartReceiveIT();
    return;
  }

  if ((g_esp8266_rx_byte != '\r') && (g_esp8266_rx_byte != '\n'))
  {
    if (g_esp8266_rx_index < (sizeof(g_esp8266_rx_line) - 1U))
    {
      g_esp8266_rx_line[g_esp8266_rx_index++] = (char)g_esp8266_rx_byte;
      g_esp8266_rx_line[g_esp8266_rx_index] = '\0';
    }
  }
  else if (g_esp8266_rx_index > 0U)
  {
    strncpy(g_esp8266_last_line, g_esp8266_rx_line, sizeof(g_esp8266_last_line) - 1U);
    g_esp8266_last_line[sizeof(g_esp8266_last_line) - 1U] = '\0';
    printf("[ESP8266] RX: %s\r\n", g_esp8266_last_line);

    if (strcmp(g_esp8266_last_line, "OK") == 0)
    {
      g_esp8266_ready = 1U;
      g_esp8266_ok_count++;
      g_esp8266_waiting_ok = 0U;
      g_esp8266_rsp_ok = 1U;
    }
    else if (strncmp(g_esp8266_last_line, "+MQTTPUB:OK", 11U) == 0)
    {
      g_esp8266_ready = 1U;
      g_esp8266_waiting_ok = 0U;
      g_esp8266_rsp_ok = 1U;
    }
    else if ((strcmp(g_esp8266_last_line, "ready") == 0) ||
             (strcmp(g_esp8266_last_line, "Ai-Thinker Technology Co. Ltd.") == 0))
    {
      g_esp8266_ready = 1U;
    }
    else if (strcmp(g_esp8266_last_line, "WIFI GOT IP") == 0)
    {
      g_esp8266_ready = 1U;
      g_esp8266_waiting_ok = 0U;
      g_esp8266_rsp_ok = 1U;
    }
    else if (strncmp(g_esp8266_last_line, "+MQTTCONNECTED:", 15U) == 0)
    {
      g_esp8266_ready = 1U;
      g_esp8266_waiting_ok = 0U;
      g_esp8266_rsp_ok = 1U;
    }
    else if (strncmp(g_esp8266_last_line, "+MQTTSUBRECV:", 12U) == 0)
    {
      const char *json_start = strchr(g_esp8266_last_line, '{');
      if (json_start != NULL)
      {
        Aliyun_HandlePropertySetMessage(json_start);
      }
    }
    else if ((g_esp8266_last_line[0] == '{') &&
             ((strstr(g_esp8266_last_line, "\"LEDSwitch0\":") != NULL) ||
              (strstr(g_esp8266_last_line, "\"LEDSwitch1\":") != NULL)))
    {
      Aliyun_HandlePropertySetMessage(g_esp8266_last_line);
    }
    else if ((strcmp(g_esp8266_last_line, "ERROR") == 0) ||
             (strcmp(g_esp8266_last_line, "FAIL") == 0) ||
             (strcmp(g_esp8266_last_line, "busy p...") == 0))
    {
      g_esp8266_waiting_ok = 0U;
      g_esp8266_rsp_error = 1U;
    }

    g_esp8266_rx_index = 0U;
    g_esp8266_rx_line[0] = '\0';
  }

  ESP8266_StartReceiveIT();
}

/* ========================= ??????? ========================= */

/* ========================= ??????? ========================= */

uint8_t AliyunIoT_IsReady(void)
{
  return (g_esp8266_step == ESP8266_STEP_DONE) ? 1U : 0U;
}

const char *AliyunIoT_GetLastLine(void)
{
  return g_esp8266_last_line;
}

uint8_t AliyunIoT_GetStep(void)
{
  return (uint8_t)g_esp8266_step;
}

uint32_t AliyunIoT_GetOkCount(void)
{
  return g_esp8266_ok_count;
}
