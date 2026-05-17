#include "usart.h"
#include <sys/unistd.h>

/* USART1 句柄由 CubeMX 在 main.c 中定义 */
extern UART_HandleTypeDef huart1;

/**
 * @brief 发送单个字符到 USART1
 * @param ch 待发送字符
 * @return int 已发送字符
 * @note
 * - 这是 syscalls.c 中 `_write()` 默认调用的底层出口
 * - 当前工程使用阻塞发送，适合调试打印
 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
    return ch;
}

/**
 * @brief 将标准库 fputc 输出重定向到 USART1
 * @param ch 待发送字符
 * @param f 文件流指针（未使用）
 * @return int 已发送字符
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    return __io_putchar(ch);
}

/**
 * @brief 将标准库 _write 输出重定向到 USART1
 * @param fd 文件描述符（未使用）
 * @param ptr 数据缓冲区
 * @param len 数据长度
 * @return int 实际发送字节数
 * @note
 * - 覆盖 CubeMX 在 syscalls.c 中提供的 weak `_write`
 * - 这样 `printf` 无论走 `_write` 还是 `fputc`，都能正确输出
 */
int _write(int fd, char *ptr, int len)
{
    (void)fd;

    if (ptr == NULL || len <= 0) {
        return 0;
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}
