#ifndef USART_H
#define USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>

/**
 * @brief 标准输出字符重定向到 USART1
 * @param ch 待发送字符
 * @return int 已发送字符
 * @note
 * - 该函数供 `printf` 重定向链路调用
 * - 依赖 `MX_USART1_UART_Init()` 已完成初始化
 */
int __io_putchar(int ch);

/**
 * @brief 标准库 `fputc` 重定向到 USART1
 * @param ch 待发送字符
 * @param f 文件流指针（未使用）
 * @return int 已发送字符
 */
int fputc(int ch, FILE *f);

#ifdef __cplusplus
}
#endif

#endif
