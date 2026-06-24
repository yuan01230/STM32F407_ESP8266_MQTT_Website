/* USER CODE BEGIN Header */
#ifndef __UDP_ECHO_H__
#define __UDP_ECHO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief 初始化 UDP 回显测试服务。
  * @note  默认监听 5000 端口，用于 PC 端验证开发板以太网收发是否正常。
  */
void UDP_Echo_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __UDP_ECHO_H__ */
/* USER CODE END Header */
