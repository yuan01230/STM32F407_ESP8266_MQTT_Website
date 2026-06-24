# LwIP 学习计划

本计划基于当前工程的实际状态：STM32F407 通过 LAN8720A/LAN8742 类 PHY 使用 RMII 接入以太网，LwIP 运行在裸机 `NO_SYS=1` 模式，当前已经完成静态 IP、ping 和 UDP Echo 基础验证。

当前固定测试环境：

- PC 有线网卡：`192.168.10.100/24`
- 开发板 ETH：`192.168.10.123/24`
- UDP Echo 端口：`5000`
- 测试前等待串口出现：`[ETH] ready: ping 192.168.10.123, UDP echo port 5000`

## 0. 七天学习完成总览

7 天学习已经从当前工程梳理、基础网络、UDP、TCP、HTTP 推进到 MQTT 发布订阅和断线重连稳定性验证。

完成后的总览文档：

- [STM32F407 LwIP 7 天学习总览](docs/lwip-learning/seven-day-lwip-learning-summary.md)

## 1. 吃透当前工程

目标：理解当前 LwIP 是如何初始化、轮询和收发数据的。

重点阅读：

- `LWIP/App/lwip.c`
  - `MX_LWIP_Init()`
  - `MX_LWIP_Process()`
  - 静态 IP、网关、掩码配置
- `LWIP/Target/ethernetif.c`
  - `low_level_init()`
  - `low_level_input()`
  - `low_level_output()`
  - `ethernet_link_check_state()`
- `LWIP/Target/lwipopts.h`
  - `NO_SYS`
  - `LWIP_NETIF_LINK_CALLBACK`
  - checksum 相关配置
- `LWIP/App/udp_echo.c`
  - `udp_new()`
  - `udp_bind()`
  - `udp_recv()`
  - `udp_sendto()`

需要回答的问题：

- `MX_LWIP_Init()` 做了哪些事情？
- 为什么主循环里必须持续调用 `MX_LWIP_Process()`？
- `ethernetif_input()` 如何把 ETH DMA 收到的数据交给 LwIP？
- 为什么必须等 `link up` 后才能稳定测试？

## 2. 理解基础网络流程

目标：能解释一次 ping 从 PC 到开发板发生了什么。

学习顺序：

1. ARP：IP 地址如何解析成 MAC 地址。
2. Ethernet Frame：MAC 帧如何承载 ARP/IP。
3. IPv4：IP 地址、子网掩码、网关的作用。
4. ICMP：ping 的请求和响应。
5. UDP：无连接收发模型。
6. TCP：连接、监听、收发、关闭。

常用验证命令：

```powershell
arp -d *
ping -S 192.168.10.100 192.168.10.123
arp -a
```

成功标准：

- ping 返回 `来自 192.168.10.123 的回复`
- `arp -a` 中出现：

```text
192.168.10.123  00-80-e1-00-00-00  动态
```

## 3. UDP 应用练习

目标：会写自己的 UDP 应用层协议。

学习文档：

- `docs/lwip-learning/stage-3-udp-application-protocol.md`

当前已有 UDP Echo：

- 开发板监听端口：`5000`
- PC 发送任意文本。
- 开发板原样返回。

PowerShell 测试：

```powershell
$u = New-Object System.Net.Sockets.UdpClient
$u.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Parse("192.168.10.100"),0))
$bytes = [Text.Encoding]::ASCII.GetBytes("hello stm32")
$u.Send($bytes,$bytes.Length,"192.168.10.123",5000)
$remote = [Net.IPEndPoint]::new([Net.IPAddress]::Any,0)
[Text.Encoding]::ASCII.GetString($u.Receive([ref]$remote))
$u.Close()
```

练习任务：

- 收到 `led on` 时点亮 LED。
- 收到 `led off` 时熄灭 LED。
- 收到 `sensor` 时返回传感器数据。
- 将返回内容改成 JSON 风格：

```json
{"temp":25.3,"humi":60.0,"light":1234}
```

需要掌握：

- `struct udp_pcb`
- `struct pbuf`
- `udp_sendto()`
- `pbuf_free()`
- 回调函数如何被 LwIP 调用。

## 4. TCP Server 练习

目标：开发板作为 TCP 服务器，PC 主动连接开发板。

学习文档：

- `docs/lwip-learning/stage-4-tcp-server.md`

建议端口：`5001`

目标交互：

```text
PC -> sensor
STM32 -> temp=25.3,humi=60.0,light=1234
```

需要掌握：

- `tcp_new()`
- `tcp_bind()`
- `tcp_listen()`
- `tcp_accept()`
- `tcp_recv()`
- `tcp_write()`
- `tcp_close()`

注意点：

- TCP 是有连接状态的，不能像 UDP 那样只看单个数据包。
- 要处理客户端断开、重复连接、发送缓冲不足等情况。
- 裸机 `NO_SYS=1` 下仍然要依赖 `MX_LWIP_Process()` 轮询推进协议栈。

## 5. 简单 HTTP 页面

目标：用浏览器访问开发板。

目标地址：

```text
http://192.168.10.123/
```

第一版只做静态/半动态 HTML：

```html
<h1>STM32 Sensor Dashboard</h1>
<p>Temperature: 25.3</p>
<p>Humidity: 60.0</p>
<p>Light: 1234</p>
```

学习重点：

- HTTP 请求格式。
- HTTP 响应头。
- TCP 连接生命周期。
- 如何把传感器数据拼接成页面。

建议先手写极简 HTTP Server，不急着引入复杂框架。

## 6. MQTT over Ethernet

目标：未来逐步用以太网替代 ESP8266 连接云端。

在进入本阶段前，应先掌握：

- DNS
- TCP Client
- MQTT 基础报文
- 阿里云 MQTT 三元组认证
- 是否需要 TLS

建议路线：

1. 先做 TCP Client 连接普通服务器。
2. 再做非 TLS MQTT。
3. 最后评估 TLS 所需 RAM/Flash 和证书管理。

当前阶段不建议马上迁移 MQTT。先把 UDP、TCP Server、HTTP 掌握扎实。

## 推荐推进顺序

1. 稳定 ping 和 UDP Echo。
2. UDP 控制 LED。
3. UDP 查询传感器数据。
4. TCP Server 查询传感器数据。
5. HTTP 页面显示传感器数据。
6. MQTT over Ethernet。

## 每次调试前检查清单

- PC 有线 IP 是 `192.168.10.100`，且状态为 `Preferred`：

```powershell
Get-NetIPAddress -InterfaceAlias "以太网" -AddressFamily IPv4
```

- 串口出现：

```text
[ETH] IP=192.168.10.123 NETMASK=255.255.255.0 GW=192.168.10.1
[ETH] link up
[ETH] ready: ping 192.168.10.123, UDP echo port 5000
```

- 清 ARP 后再测：

```powershell
arp -d *
ping -S 192.168.10.100 192.168.10.123
```

- 如果 ping 失败，查看 ARP：

```powershell
arp -a
```

如果没有 `192.168.10.123 -> 00-80-e1-00-00-00`，说明还停在 ARP 阶段。
