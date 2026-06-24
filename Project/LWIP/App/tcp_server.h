/* USER CODE BEGIN Header */
#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief 初始化 TCP 命令服务器。
  * @note  默认监听 5001 端口，用于第四阶段学习 TCP raw API。
  */
void TCP_Server_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCP_SERVER_H__ */
/* USER CODE END Header */
