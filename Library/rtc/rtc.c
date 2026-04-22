//
// Created by 标语 on 26-3-1.
//

#include "rtc.h"
#include "stdio.h"
#include "../../Library/led/led.h"
#include "../../Library/tftlcd/tftlcd.h"

// 外部引用 LCD 相关变量
extern _tftlcd_data tftlcd_data;
extern u16 FRONT_COLOR;
extern u16 BACK_COLOR;

// 标志位
static volatile uint8_t rtc_alarm_flag = 0;
static volatile uint8_t rtc_wakeup_flag = 0;
static volatile uint8_t rtc_timestamp_flag = 0;

/**
  * @brief  RTC 闹钟 A 事件回调函数
  * @param  hrtc: RTC 句柄指针
  * @note   当 RTC 闹钟 A 触发时，HAL 库会自动调用此函数。
  *         在此函数中设置标志位并翻转 LED1 以指示闹钟触发。
  */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
    if(hrtc->Instance == RTC)
    {
        rtc_alarm_flag = 1;
        LED_Toggle(LED1); // Alarm 触发翻转 LED1
    }
}

/**
  * @brief  RTC 唤醒定时器事件回调函数
  * @param  hrtc: RTC 句柄指针
  * @note   当 RTC 唤醒定时器计数结束时，HAL 库会自动调用此函数。
  *         在此函数中设置标志位并翻转 LED0 以指示唤醒事件。
  */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    if(hrtc->Instance == RTC)
    {
        rtc_wakeup_flag = 1;
        LED_Toggle(LED0); // WakeUp 触发翻转 LED0
    }
}

/**
  * @brief  RTC 时间戳事件回调函数
  * @param  hrtc: RTC 句柄指针
  * @note   当检测到时间戳引脚（通常是 PC13）触发时，HAL 库会自动调用此函数。
  *         在此函数中设置标志位。
  */
void HAL_RTCEx_TimeStampEventCallback(RTC_HandleTypeDef *hrtc)
{
    if(hrtc->Instance == RTC)
    {
        rtc_timestamp_flag = 1;
        // 这里可以加个 LED 翻转或者其他指示，比如 LED1
        // LED_Toggle(LED1);
    }
}

/**
  * @brief  检查并清除 RTC 闹钟标志位
  * @retval 1: 闹钟已触发
  * @retval 0: 闹钟未触发
  * @note   主循环中调用此函数来检测闹钟事件，调用后会自动清除标志位。
  */
uint8_t RTC_Check_Alarm_Flag(void)
{
    if (rtc_alarm_flag)
    {
        rtc_alarm_flag = 0;
        return 1;
    }
    return 0;
}

/**
  * @brief  检查并清除 RTC 唤醒标志位
  * @retval 1: 唤醒事件已触发
  * @retval 0: 唤醒事件未触发
  * @note   主循环中调用此函数来检测唤醒事件，调用后会自动清除标志位。
  */
uint8_t RTC_Check_WakeUp_Flag(void)
{
    if (rtc_wakeup_flag)
    {
        rtc_wakeup_flag = 0;
        return 1;
    }
    return 0;
}

/**
  * @brief  检查并清除 RTC 时间戳标志位
  * @retval 1: 时间戳事件已触发
  * @retval 0: 时间戳事件未触发
  * @note   主循环中调用此函数来检测时间戳事件，调用后会自动清除标志位。
  */
uint8_t RTC_Check_Timestamp_Flag(void)
{
    if (rtc_timestamp_flag)
    {
        rtc_timestamp_flag = 0;
        return 1;
    }
    return 0;
}

/**
  * @brief  开启 RTC 唤醒定时器
  * @param  seconds: 唤醒周期（秒）
  * @note   配置 RTC 唤醒定时器以 1Hz (CK_SPRE_16BITS) 频率计数。
  *         如果 seconds 为 0，则强制设为 1。
  */
void RTC_WakeUp_Start(uint32_t seconds)
{
    if (seconds == 0) seconds = 1;
    // Counter = seconds - 1 (因为是从 0 开始计数到 Counter 触发)
    // Clock = 1Hz (CK_SPRE_16BITS)
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds - 1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK)
    {
        printf("[RTC] WakeUp Start Failed!\r\n");
    }
    else
    {
        printf("[RTC] WakeUp Started: %lu s\r\n", (unsigned long)seconds);
    }
}

/**
  * @brief  关闭 RTC 唤醒定时器
  * @note   禁用 RTC 唤醒定时器功能。
  */
void RTC_WakeUp_Stop(void)
{
    if (HAL_RTCEx_DeactivateWakeUpTimer(&hrtc) != HAL_OK)
    {
        printf("[RTC] WakeUp Stop Failed!\r\n");
    }
    else
    {
        printf("[RTC] WakeUp Stopped!\r\n");
    }
}

/**
  * @brief  设置 RTC 闹钟 A
  * @param  week: 星期几 (1-7, 1=Monday)
  * @param  hour: 小时 (0-23)
  * @param  min:  分钟 (0-59)
  * @param  sec:  秒 (0-59)
  * @note   配置闹钟 A 在指定的星期、时、分、秒触发。
  */
void RTC_Set_Alarm_A(uint8_t week, uint8_t hour, uint8_t min, uint8_t sec)
{
    RTC_AlarmTypeDef sAlarm = {0};
    sAlarm.AlarmTime.Hours = hour;
    sAlarm.AlarmTime.Minutes = min;
    sAlarm.AlarmTime.Seconds = sec;
    sAlarm.AlarmTime.SubSeconds = 0;
    sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
    sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
    sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_WEEKDAY;
    sAlarm.AlarmDateWeekDay = week;
    sAlarm.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD);
}

/**
  * @brief  获取当前 RTC 时间字符串
  * @param  buffer: 存储时间字符串的缓冲区 (格式: "HH:MM:SS")
  * @note   调用 HAL_RTC_GetTime 获取当前时间并格式化为字符串。
  */
void RTC_Get_Time_String(char *buffer)
{
    RTC_TimeTypeDef sTime;
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    sprintf(buffer, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
}

/**
  * @brief  获取当前 RTC 日期字符串
  * @param  buffer: 存储日期字符串的缓冲区 (格式: "YYYY-MM-DD")
  * @note   调用 HAL_RTC_GetDate 获取当前日期并格式化为字符串。
  */
void RTC_Get_Date_String(char *buffer)
{
    RTC_DateTypeDef sDate;
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    sprintf(buffer, "20%02d-%02d-%02d", sDate.Year, sDate.Month, sDate.Date);
}

/**
  * @brief  获取硬件锁存的时间戳数据
  * @param  time_buf: 存储时间字符串的缓冲区 (格式: "HH:MM:SS")
  * @param  date_buf: 存储日期字符串的缓冲区 (格式: "YYYY-MM-DD")
  * @note   调用 HAL_RTCEx_GetTimeStamp 获取锁存的时间和日期。
  */
void RTC_Get_Timestamp_String(char *time_buf, char *date_buf)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    // 获取时间戳寄存器中的时间
    HAL_RTCEx_GetTimeStamp(&hrtc, &sTime, &sDate, RTC_FORMAT_BIN);

    if (time_buf)
    {
        sprintf(time_buf, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    }
    if (date_buf)
    {
        sprintf(date_buf, "20%02d-%02d-%02d", sDate.Year, sDate.Month, sDate.Date);
    }
}

/**
  * @brief  检查 RTC 是否已经配置过 (检查后备寄存器)
  * @retval 1: 已配置 (无需重新设置时间)
  * @retval 0: 未配置 (首次上电)
  * @note   读取后备寄存器 RTC_BKP_DRX，如果值等于 RTC_BKP_DATA，说明已经配置过。
  */
uint8_t RTC_Check_Configured_Flag(void)
{
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DRX) == RTC_BKP_DATA)
    {
        return 1;
    }
    return 0;
}

/**
  * @brief  设置 RTC 已配置标志 (写入后备寄存器)
  * @note   向后备寄存器 RTC_BKP_DRX 写入 RTC_BKP_DATA。
  */
void RTC_Set_Configured_Flag(void)
{
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DRX, RTC_BKP_DATA);
}
