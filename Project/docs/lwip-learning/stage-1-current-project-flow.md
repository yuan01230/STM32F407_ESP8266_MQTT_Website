# 阶段 1：吃透当前工程的 LwIP 调用逻辑

本阶段目标不是新增功能，而是看懂当前工程里的 LwIP 如何启动、如何轮询、如何从网口收包、如何把数据交给 UDP Echo 应用。

当前工程运行模式：

- MCU：STM32F407
- PHY：LAN8720A/LAN8742 类 RMII PHY
- LwIP 模式：裸机 `NO_SYS=1`
- 开发板 IP：`192.168.10.123`
- PC 有线 IP：`192.168.10.100`
- UDP Echo 端口：`5000`

## 1. 本阶段要掌握什么

学习完成后，你应该能回答：

- `MX_LWIP_Init()` 做了什么？
- 为什么主循环必须一直调用 `MX_LWIP_Process()`？
- `netif_add()` 为什么要传入 `ethernetif_init` 和 `ethernet_input`？
- `ethernetif_input()` 如何把 ETH DMA 收到的数据交给 LwIP？
- `low_level_output()` 如何把 LwIP 要发送的数据交给 ETH DMA？
- 为什么必须等串口打印 `link up` / `ready` 后再测试 ping？

## 2. 代码入口总览

主要文件：

- `Core/Src/main.c`
- `LWIP/App/lwip.c`
- `LWIP/Target/ethernetif.c`
- `LWIP/App/udp_echo.c`
- `LWIP/Target/lwipopts.h`

整体关系：

```mermaid
flowchart TD
    A["main.c<br/>main()"] --> B["初始化 GPIO/DMA/USART/SDIO 等外设"]
    B --> C["ETH_PHY_Reset()<br/>释放 PHY 硬件复位"]
    C --> D["MX_LWIP_Init()<br/>初始化 LwIP 和网卡"]
    D --> E["while(1)"]
    E --> F["MX_LWIP_Process()<br/>裸机轮询协议栈"]
    F --> E
```

关键点：

- `ETH_PHY_Reset()` 必须在 `MX_LWIP_Init()` 之前执行，否则 PHY 可能一直被硬件下拉保持复位。
- 当前没有 FreeRTOS，所以 LwIP 不会自己跑线程，必须靠 `while(1)` 里持续调用 `MX_LWIP_Process()`。

## 3. 初始化调用逻辑

`MX_LWIP_Init()` 位于 `LWIP/App/lwip.c`。

初始化流程：

```mermaid
flowchart TD
    A["MX_LWIP_Init()"] --> B["设置静态 IP<br/>192.168.10.123"]
    B --> C["lwip_init()<br/>初始化 LwIP 内部模块"]
    C --> D["IP4_ADDR()<br/>生成 ipaddr/netmask/gw"]
    D --> E["netif_add()"]
    E --> F["ethernetif_init()<br/>初始化 ETH MAC/PHY/DMA"]
    E --> G["ethernet_input<br/>作为以太网帧分发入口"]
    F --> H["netif_set_default()"]
    G --> H
    H --> I["netif_set_up()"]
    I --> J["netif_set_link_callback()"]
    J --> K["UDP_Echo_Init()<br/>绑定 UDP 5000"]
```

`netif_add()` 是非常关键的一步。它把 LwIP 的“虚拟网卡对象” `gnetif` 和底层硬件驱动连接起来。

简化理解：

```text
gnetif = LwIP 眼中的网卡
ethernetif_init = 这张网卡怎么初始化硬件
ethernet_input = 收到以太网帧后如何交给 LwIP 分发
```

## 4. 裸机轮询逻辑

`MX_LWIP_Process()` 位于 `LWIP/App/lwip.c`，主循环一直调用它。

```mermaid
flowchart TD
    A["while(1)"] --> B["MX_LWIP_Process()"]
    B --> C["ethernetif_input(&gnetif)<br/>取 ETH DMA 收到的包"]
    B --> D["sys_check_timeouts()<br/>处理 ARP/TCP 等定时器"]
    B --> E["Ethernet_Link_Periodic_Handle()<br/>每 100ms 检查 PHY link"]
    E --> F["ethernet_link_check_state()"]
    F --> G{"PHY link 状态"}
    G -->|"link up"| H["配置 MAC 速率/双工<br/>HAL_ETH_Start_IT()<br/>netif_set_link_up()"]
    G -->|"link down"| I["HAL_ETH_Stop_IT()<br/>netif_set_link_down()"]
```

如果 `MX_LWIP_Process()` 停止调用，会出现：

- 收到的数据包不再被取出。
- ARP/TCP 等定时器不再推进。
- link up/down 状态不能及时同步。
- UDP Echo 不响应。

## 5. Link Up 为什么重要

`ethernet_link_check_state()` 位于 `LWIP/Target/ethernetif.c`。

它负责把 PHY 的物理链路状态同步给 STM32 ETH MAC 和 LwIP。

```mermaid
flowchart TD
    A["ethernet_link_check_state()"] --> B["LAN8742_GetLinkState()"]
    B --> C{"链路是否已连接"}
    C -->|"未连接"| D["停止 ETH<br/>netif link down<br/>打印 link down"]
    C -->|"已连接"| E["读取速度和双工模式"]
    E --> F["HAL_ETH_GetMACConfig()"]
    F --> G["设置 MACConf.Speed / DuplexMode"]
    G --> H["HAL_ETH_SetMACConfig()"]
    H --> I["HAL_ETH_Start_IT()"]
    I --> J["netif_set_up()"]
    J --> K["netif_set_link_up()"]
    K --> L["打印 link up / ready"]
```

测试前必须等串口出现：

```text
[ETH] link up: 10M half duplex
[ETH] ready: ping 192.168.10.123, UDP echo port 5000
```

否则 MAC/DMA 可能还没有真正开始收发，PC 的 ARP 请求不会得到回应。

## 6. 收包调用逻辑

当 PC 发 ARP、ping、UDP 数据时，数据从网口进入。

```mermaid
sequenceDiagram
    participant PC as PC
    participant PHY as LAN8720A PHY
    participant MAC as STM32 ETH MAC/DMA
    participant EIF as ethernetif.c
    participant LWIP as LwIP
    participant APP as UDP Echo

    PC->>PHY: 以太网帧
    PHY->>MAC: RMII 信号
    MAC->>EIF: DMA 写入 RX 描述符
    EIF->>EIF: ethernetif_input()
    EIF->>EIF: low_level_input()
    EIF->>MAC: HAL_ETH_ReadData()
    EIF->>LWIP: netif->input(p, netif)
    LWIP->>LWIP: ethernet_input()
    LWIP->>LWIP: 分发 ARP / IPv4 / ICMP / UDP
    LWIP->>APP: UDP 5000 调用 UDP_Echo_Receive()
```

关键函数：

- `ethernetif_input()`
- `low_level_input()`
- `HAL_ETH_ReadData()`
- `netif->input(p, netif)`
- `ethernet_input()`

## 7. 发包调用逻辑

当开发板要回复 ARP、ping、UDP Echo 时，数据从 LwIP 发回网口。

```mermaid
sequenceDiagram
    participant APP as UDP Echo / ICMP / ARP
    participant LWIP as LwIP
    participant EIF as ethernetif.c
    participant MAC as STM32 ETH MAC/DMA
    participant PHY as LAN8720A PHY
    participant PC as PC

    APP->>LWIP: udp_sendto() 或协议栈自动回包
    LWIP->>EIF: low_level_output()
    EIF->>EIF: pbuf 链转 ETH_BufferTypeDef
    EIF->>MAC: HAL_ETH_Transmit()
    EIF->>MAC: HAL_ETH_ReleaseTxPacket()
    MAC->>PHY: RMII 发送
    PHY->>PC: 以太网帧
```

关键函数：

- `udp_sendto()`
- `low_level_output()`
- `HAL_ETH_Transmit()`
- `HAL_ETH_ReleaseTxPacket()`

注意：当前工程已经修复了 TX 描述符释放问题。阻塞发送后会调用 `HAL_ETH_ReleaseTxPacket()`，避免发送描述符长期占用。

## 8. UDP Echo 调用逻辑

`UDP_Echo_Init()` 位于 `LWIP/App/udp_echo.c`。

```mermaid
flowchart TD
    A["UDP_Echo_Init()"] --> B["udp_new()<br/>创建 UDP PCB"]
    B --> C["udp_bind(IP_ADDR_ANY, 5000)<br/>监听 5000 端口"]
    C --> D["udp_recv(UDP_Echo_Receive)<br/>注册接收回调"]
    D --> E["收到 UDP 数据"]
    E --> F["UDP_Echo_Receive()"]
    F --> G["udp_sendto()<br/>原样回发"]
    G --> H["pbuf_free()<br/>释放接收缓冲"]
```

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

预期返回：

```text
hello stm32
```

## 9. 本阶段调试清单

每次测试前：

```powershell
Get-NetIPAddress -InterfaceAlias "以太网" -AddressFamily IPv4
```

确认：

```text
IPAddress    : 192.168.10.100
AddressState : Preferred
```

串口确认：

```text
[ETH] IP=192.168.10.123 NETMASK=255.255.255.0 GW=192.168.10.1
[ETH] link up
[ETH] ready: ping 192.168.10.123, UDP echo port 5000
```

ping 测试：

```powershell
arp -d *
ping -S 192.168.10.100 192.168.10.123
arp -a
```

成功标志：

```text
来自 192.168.10.123 的回复
192.168.10.123  00-80-e1-00-00-00  动态
```

## 10. 阶段总结

当前工程的主调用链可以记成：

```text
main()
  -> ETH_PHY_Reset()
  -> MX_LWIP_Init()
      -> lwip_init()
      -> netif_add(... ethernetif_init, ethernet_input)
      -> UDP_Echo_Init()
  -> while(1)
      -> MX_LWIP_Process()
          -> ethernetif_input()
          -> sys_check_timeouts()
          -> Ethernet_Link_Periodic_Handle()
```

只要理解这条链路，就已经掌握了当前裸机 LwIP 工程最核心的运行方式。
