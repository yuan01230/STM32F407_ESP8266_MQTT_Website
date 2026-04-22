//
// Created by 标语 on 26-1-2.
//
// syscalls.c
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include "usart.h"
#include "stdio.h"

extern UART_HandleTypeDef huart1;

int _write(int fd, char *ptr, int len)

{

    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, 0xFFFF);

    return len;

}

