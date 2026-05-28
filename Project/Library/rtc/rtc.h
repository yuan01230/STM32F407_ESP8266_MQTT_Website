//
// Created by 标语 on 26-3-1.
//

#ifndef RTC_H
#define RTC_H

#include "main.h"

// 声明外部 RTC 句柄
extern RTC_HandleTypeDef hrtc;

// RTC 后备寄存器定义
#define RTC_BKP_DRX     RTC_BKP_DR0  // 使用后备寄存器 0
#define RTC_BKP_DATA    0x32F2       // 魔术数 (标记时间已设置)

/**
  * @brief  RTC 闹钟 A 事件回调函数
  * @param  hrtc: RTC 句柄指针
  */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc);

/**
  * @brief  RTC 唤醒定时器事件回调函数
  * @param  hrtc: RTC 句柄指针
  */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc);

/**
  * @brief  RTC 时间戳事件回调函数
  * @param  hrtc: RTC 句柄指针
  */
void HAL_RTCEx_TimeStampEventCallback(RTC_HandleTypeDef *hrtc);

/**
  * @brief  检查并清除 RTC 闹钟标志位
  * @retval 1: 闹钟已触发
  * @retval 0: 闹钟未触发
  */
uint8_t RTC_Check_Alarm_Flag(void);

/**
  * @brief  检查并清除 RTC 唤醒标志位
  * @retval 1: 唤醒事件已触发
  * @retval 0: 唤醒事件未触发
  */
uint8_t RTC_Check_WakeUp_Flag(void);

/**
  * @brief  检查并清除 RTC 时间戳标志位
  * @retval 1: 时间戳事件已触发
  * @retval 0: 时间戳事件未触发
  */
uint8_t RTC_Check_Timestamp_Flag(void);

/**
  * @brief  开启 RTC 唤醒定时器
  * @param  seconds: 唤醒周期（秒）
  */
void RTC_WakeUp_Start(uint32_t seconds);

/**
  * @brief  关闭 RTC 唤醒定时器
  */
void RTC_WakeUp_Stop(void);

/**
  * @brief  设置 RTC 闹钟 A
  * @param  week: 星期几 (1-7, 1=Monday)
  * @param  hour: 小时 (0-23)
  * @param  min:  分钟 (0-59)
  * @param  sec:  秒 (0-59)
  */
void RTC_Set_Alarm_A(uint8_t week, uint8_t hour, uint8_t min, uint8_t sec);

/**
  * @brief  获取当前 RTC 时间字符串
  * @param  buffer: 存储时间字符串的缓冲区 (格式: "HH:MM:SS")
  */
void RTC_Get_Time_String(char *buffer);

/**
  * @brief  获取当前 RTC 日期字符串
  * @param  buffer: 存储日期字符串的缓冲区 (格式: "YYYY-MM-DD")
  */
void RTC_Get_Date_String(char *buffer);

/**
  * @brief  获取硬件锁存的时间戳数据
  * @param  time_buf: 存储时间字符串的缓冲区 (格式: "HH:MM:SS")
  * @param  date_buf: 存储日期字符串的缓冲区 (格式: "YYYY-MM-DD")
  */
void RTC_Get_Timestamp_String(char *time_buf, char *date_buf);

/**
  * @brief  检查 RTC 是否已经配置过 (检查后备寄存器)
  * @retval 1: 已配置 (无需重新设置时间)
  * @retval 0: 未配置 (首次上电)
  */
uint8_t RTC_Check_Configured_Flag(void);

/**
  * @brief  设置 RTC 已配置标志 (写入后备寄存器)
  */
void RTC_Set_Configured_Flag(void);

#endif //RTC_H
