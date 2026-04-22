//
// Created by 标语 on 26-1-2.
//

#ifndef KEY_H
#define KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum {
    KEY_NONE = -1,
    KEY0 = 0,
    KEY1 = 1,
    KEY2 = 2,
    KEY_UP = 3
} KeyName_t;

// 调用此函数扫描按键（建议每 5~10ms 调用一次）
KeyName_t Key_Scan(void);

#endif //KEY_H
