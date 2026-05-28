# Website - 阿里云 IoT 仪表盘

这个目录存放网站和桌面启动器，用来展示设备数据、切换设备、配置阿里云 OpenAPI，并在本地模拟模式下运行。

## 这个网站做什么

- 展示温湿度、光照、电压、姿态等数据
- 显示设备在线状态和最近更新时间
- 支持本地模拟模式
- 支持阿里云 OpenAPI 接入
- 支持设备控制指令下发
- 支持桌面端打包运行

## 目录结构

网站源码实际位于：

- `Website/aliyun-iot-dashboard/client/`：前端页面
- `Website/aliyun-iot-dashboard/server/`：后端服务
- `Website/aliyun-iot-dashboard/desktop/`：Electron 桌面启动器

## 如何使用

### 1. 安装依赖

进入网站项目目录后安装依赖：

```bash
cd Website/aliyun-iot-dashboard
npm install
```

如果你只想单独启动后端服务，也可以进入 `server/` 目录安装服务端依赖。

### 2. 启动桌面版

在网站项目目录下运行：

```bash
npm run desktop
```

桌面程序会自动拉起本地服务，并在 `http://127.0.0.1:5000` 打开仪表盘。

### 3. 单独启动后端

如果你想直接看网页服务，可以进入服务端目录：

```bash
cd Website/aliyun-iot-dashboard/server
npm install
npm start
```

然后访问：

```text
http://localhost:5000
```

### 4. 打包

如果需要生成 Windows 便携版，可以运行：

```bash
npm run pack
```

## 本地模拟模式

如果没有配置阿里云 OpenAPI 凭证，网站会进入本地模拟模式：

- 页面继续显示模拟数据
- 设备状态由本地服务维护
- 控制操作只影响本地状态
- 不会真实连接阿里云

## 阿里云接入

在前端配置面板中填写：

- `AccessKey ID`
- `AccessKey Secret`
- `Region ID`

配置成功后，网站会尝试读取真实设备状态并同步显示。

## 注意事项

- 网站默认后端端口是 `5000`
- 桌面端和后端都依赖本地 `server/` 目录
- 打包产物和 `node_modules/` 都是可重建内容，不需要纳入核心源码维护
