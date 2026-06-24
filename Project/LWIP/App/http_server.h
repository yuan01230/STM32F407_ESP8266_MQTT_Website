/* USER CODE BEGIN Header */
#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief 初始化最小 HTTP 服务器。
  * @note  默认监听 80 端口，浏览器访问 http://192.168.10.123/。
  */
void HTTP_Server_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __HTTP_SERVER_H__ */
/* USER CODE END Header */
