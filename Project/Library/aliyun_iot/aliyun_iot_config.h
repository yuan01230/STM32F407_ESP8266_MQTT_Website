#ifndef ALIYUN_IOT_CONFIG_H
#define ALIYUN_IOT_CONFIG_H

#if __has_include("aliyun_iot_config.local.h")
#include "aliyun_iot_config.local.h"
#else
/*
 * Local-only Wi-Fi / Aliyun IoT settings.
 * Fill these values on your machine before building and keep real credentials out of git.
 */
#define ALIYUN_IOT_WIFI_SSID            "YOUR_WIFI_SSID"
#define ALIYUN_IOT_WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"

#define ALIYUN_IOT_MQTT_HOST            "YOUR_MQTT_HOST"
#define ALIYUN_IOT_MQTT_PORT            1883U
#define ALIYUN_IOT_MQTT_CLIENT_ID_AT    "YOUR_PRODUCT_KEY.YOUR_DEVICE_NAME|securemode=2\\,signmethod=hmacsha256\\,timestamp=YOUR_TIMESTAMP|"
#define ALIYUN_IOT_MQTT_USERNAME        "YOUR_DEVICE_NAME&YOUR_PRODUCT_KEY"
#define ALIYUN_IOT_MQTT_PASSWORD        "YOUR_MQTT_PASSWORD"

#define ALIYUN_IOT_PROP_POST_TOPIC      "/sys/YOUR_PRODUCT_KEY/YOUR_DEVICE_NAME/thing/event/property/post"
#define ALIYUN_IOT_PROP_SET_TOPIC       "/sys/YOUR_PRODUCT_KEY/YOUR_DEVICE_NAME/thing/service/property/set"
#define ALIYUN_IOT_PROP_SET_REPLY_TOPIC "/sys/YOUR_PRODUCT_KEY/YOUR_DEVICE_NAME/thing/service/property/set_reply"

#define ALIYUN_IOT_PROP_POST_INTERVAL_MS 10000U
#endif

#endif
