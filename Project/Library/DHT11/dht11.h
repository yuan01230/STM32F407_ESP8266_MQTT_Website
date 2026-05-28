#ifndef DHT11_H
#define DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief DHT11 采样结果
 * @note
 * - DHT11 固定输出 5 字节数据：湿度整数/小数、温度整数/小数、校验和。
 * - 对 DHT11 本体而言，小数位通常为 0，但仍保留字段以兼容原始协议格式。
 */
typedef struct
{
    uint8_t humi_int;   /**< 湿度整数部分 */
    uint8_t humi_dec;   /**< 湿度小数部分 */
    uint8_t temp_int;   /**< 温度整数部分 */
    uint8_t temp_dec;   /**< 温度小数部分 */
    uint8_t checksum;   /**< 原始校验和字节 */
} DHT11_Data_t;

/**
 * @brief 初始化 DHT11 驱动
 * @note
 * - 该函数不负责 GPIO 时钟初始化，需由 `MX_GPIO_Init()` 提前完成。
 * - 初始化完成后，数据引脚会保持输入上拉状态，等待后续采样。
 */
void DHT11_Init(void);

/**
 * @brief 读取一次 DHT11 数据
 * @param data 输出结果缓冲区
 * @return uint8_t 0=成功，1=失败
 * @note
 * - 若 `data==NULL`，函数直接返回失败。
 * - 失败可能由无响应、时序超时或校验和错误导致。
 * - DHT11 建议采样周期不小于 1 秒。
 */
uint8_t DHT11_ReadData(DHT11_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif
