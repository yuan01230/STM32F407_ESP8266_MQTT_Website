# STM32F407 Ethernet / LwIP / MQTT 学习分支

当前分支：`codex/Ethernet`

这个分支用于记录 STM32F407 以太网模块、LwIP 协议栈、UDP/TCP/HTTP/MQTT 以及 MQTT 稳定性验证的学习过程。

和 `master` 分支不同，当前分支的重点不是阿里云 IoT 网站联动，而是通过本地以太网链路把 STM32 网络能力从基础收发一步步打通到 MQTT 发布订阅和断线重连。

## 当前分支完成内容

- STM32F407 通过 PHY 使用 RMII 接入以太网
- LwIP 裸机 `NO_SYS=1` 轮询模式运行
- 静态 IP：`192.168.10.123`
- PC 有线网卡：`192.168.10.100/24`
- UDP Echo / UDP 命令协议
- TCP Server 命令服务
- TCP Client 连接 PC Server 实验
- HTTP Server 简单网页实验
- 本地 EMQX Broker + MQTTX 测试环境
- STM32 MQTT Client：CONNECT、SUBSCRIBE、PUBLISH、PINGREQ
- `stm32/sensor` 传感器数据发布
- `stm32/cmd` LED 命令订阅
- `stm32/status` 命令执行状态回传
- Broker 重启、Broker 关闭再开启、网线断开再插回后的 MQTT 恢复验证

## 重要结论

当前工程使用 `NO_SYS=1` 和 `MX_LWIP_Process()` 轮询推进网络处理。

因此以太网启动方式保留为：

```c
HAL_ETH_Start(&heth);
```

不要改回：

```c
HAL_ETH_Start_IT(&heth);
```

本分支排查过一次关键问题：`HAL_ETH_Start_IT()` 与当前轮询收包模型不匹配时，可能出现 ARP 表存在但 ping/MQTT 异常的问题。最终切回 `HAL_ETH_Start(&heth)` 后，ARP、ICMP、MQTT 全部恢复。

## 学习文档入口

7 天学习总览：

- [STM32F407 LwIP 7 天学习总览](Project/docs/lwip-learning/seven-day-lwip-learning-summary.md)

分阶段文档：

- [阶段 1：吃透当前工程流程](Project/docs/lwip-learning/stage-1-current-project-flow.md)
- [阶段 2：理解基础网络流程](Project/docs/lwip-learning/stage-2-basic-network-flow.md)
- [阶段 3：UDP 应用协议](Project/docs/lwip-learning/stage-3-udp-application-protocol.md)
- [阶段 4：TCP Server](Project/docs/lwip-learning/stage-4-tcp-server.md)
- [阶段 5：HTTP Server](Project/docs/lwip-learning/stage-5-simple-http-page.md)
- [阶段 6：MQTT over Ethernet](Project/docs/lwip-learning/stage-6-mqtt-over-ethernet.md)
- [阶段 7：MQTT 断线重连与状态机工程化](Project/docs/lwip-learning/stage-7-mqtt-reconnect-state-machine.md)

## 主要代码入口

- `Project/LWIP/App/lwip.c`：LwIP 初始化和主循环处理入口
- `Project/LWIP/Target/ethernetif.c`：以太网底层收发、PHY link 检测、ARP/ICMP 调试
- `Project/LWIP/App/udp_echo.c`：UDP Echo 和 UDP 命令协议
- `Project/LWIP/App/tcp_server.c`：TCP Server 命令服务
- `Project/LWIP/App/tcp_client.c`：STM32 作为 TCP Client 的验证代码
- `Project/LWIP/App/http_server.c`：简单 HTTP Server
- `Project/LWIP/App/mqtt_client.c`：MQTT Client、发布订阅、重连状态机

## 本地 MQTT 测试环境

EMQX 使用 Docker 运行：

```powershell
docker run -d --name emqx `
  -p 1883:1883 `
  -p 18083:18083 `
  emqx/emqx:latest
```

Dashboard：

```text
http://localhost:18083
```

MQTT Broker：

```text
192.168.10.100:1883
```

MQTTX 常用 topic：

| topic | 方向 | 说明 |
| --- | --- | --- |
| `stm32/sensor` | STM32 -> MQTTX | 传感器 JSON 数据 |
| `stm32/cmd` | MQTTX -> STM32 | LED 控制命令 |
| `stm32/status` | STM32 -> MQTTX | 命令执行状态回传 |

## 常用验证命令

基础链路验证：

```powershell
arp -d *
ping -S 192.168.10.100 192.168.10.123
arp -a
```

Broker 重启验证：

```powershell
docker restart emqx
```

查看 EMQX 容器：

```powershell
docker ps
```

## 当前验证结论

本分支已经完成以下稳定性实验：

- 正常上电连接、发布、订阅、LED 控制
- Docker Restart 重启 EMQX 的恢复观察
- Broker 关闭再开启后的自动恢复
- 网线断开再插回后的自动恢复

结论：当前 MQTT Client 已经具备基本工程稳定性，能在 Broker 异常和物理链路异常后重新连接并恢复发布订阅流程。

## 后续建议

下一阶段可以继续学习：

- MQTT QoS 1
- MQTT Retain
- MQTT Last Will
- JSON 命令协议升级
- 长时间运行压力测试
- 从本地 EMQX 迁移到云平台 MQTT
