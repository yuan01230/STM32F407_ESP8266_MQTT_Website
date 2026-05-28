const express = require("express");
const cors = require("cors");
const dotenv = require("dotenv");
const path = require("path");
const fs = require("fs");

dotenv.config();

const app = express();
const PORT = Number(process.env.PORT || 5000);
const clientDir = process.env.ELECTRON_DESKTOP_RUNTIME && process.resourcesPath
  ? path.join(process.resourcesPath, "client")
  : path.join(__dirname, "../client");
const dataDir = process.env.ELECTRON_DESKTOP_RUNTIME && process.env.APPDATA
  ? path.join(process.env.APPDATA, "Aliyun IoT Dashboard")
  : __dirname;
const stateFile = path.join(dataDir, "state.json");

app.use(cors());
app.use(express.json());
app.use("/api", (_req, res, next) => {
  res.set("Cache-Control", "no-store, no-cache, must-revalidate, private");
  next();
});
app.use(express.static(clientDir));

app.get("/", (_req, res) => {
  res.sendFile(path.join(clientDir, "index.html"));
});

const devConfig = {
  productKey: process.env.PRODUCT_KEY || "YOUR_PRODUCT_KEY",
  deviceName: process.env.DEVICE_NAME || "YOUR_DEVICE_NAME",
  iotInstanceId: process.env.IOT_INSTANCE_ID || "YOUR_IOT_INSTANCE_ID",
  mqttHostUrl: process.env.MQTT_HOST_URL || "YOUR_MQTT_HOST"
};

let openApiConfig = {
  accessKeyId: process.env.ACCESS_KEY_ID || "",
  accessKeySecret: process.env.ACCESS_KEY_SECRET || "",
  regionId: process.env.REGION_ID || "cn-shanghai"
};

let lastCommandTimes = {
  LEDSwitch0: 0,
  LEDSwitch1: 0,
  Light_Out_Voltage: 0
};

let lastCommandValues = {
  LEDSwitch0: 0,
  LEDSwitch1: 0,
  Light_Out_Voltage: 0
};

let apiLogBuffer = [];
const LOG_RETENTION_MS = 10 * 60 * 1000;
const TELEMETRY_ONLINE_MAX_AGE_SECONDS = 60;
let stateLoaded = false;

function trimApiLogBuffer() {
  const cutoff = Date.now() - LOG_RETENTION_MS;
  apiLogBuffer = apiLogBuffer.filter((entry) => entry.timestamp >= cutoff);
}

function addApiLog(type, message) {
  const line = `[${new Date().toLocaleTimeString()}] [${type}] ${message}`;
  apiLogBuffer.push({
    timestamp: Date.now(),
    line
  });
  trimApiLogBuffer();
  console.log(line);
}

function createSensorDevice(overrides = {}) {
  return {
  id: "Sensor",
  name: "阿里云终端 Sensor",
  productKey: devConfig.productKey,
  deviceName: devConfig.deviceName,
  type: "Sensor",
  status: "offline",
  lastOfflineAt: null,
  lastUpdate: new Date().toISOString(),
  telemetryLastReportedAt: null,
  telemetryStale: true,
  telemetryAgeSeconds: 0,
  metrics: {
    LEDSwitch0: 0,
    LEDSwitch1: 0,
    temperature: 0,
    Humidity: 0,
    Yaw: 0,
    Roll: 0,
    Pitch: 0,
    Light_ADC: 0,
    Light_Out_Voltage: 0
  },
  history: [],
  ...overrides
  };
}

function normalizeDevice(device) {
  if (!device || !device.id) return null;
  const base = device.type === "Sensor"
    ? createSensorDevice()
    : {
        id: device.id,
        name: device.name || device.id,
        productKey: "",
        deviceName: "",
        type: "Generic",
        status: "offline",
        lastOfflineAt: null,
        lastUpdate: new Date().toISOString(),
        telemetryLastReportedAt: null,
        telemetryStale: true,
        telemetryAgeSeconds: 0,
        metrics: { Voltage: 0, Current: 0, Power: 0, Alarm: 0 },
        history: []
      };

  return {
    ...base,
    ...device,
    status: "offline",
    lastOfflineAt: device.lastOfflineAt || null,
    telemetryStale: true,
    metrics: {
      ...base.metrics,
      ...(device.metrics || {}),
      ...(device.type === "Sensor" ? { LEDSwitch0: 0, LEDSwitch1: 0 } : {})
    },
    history: Array.isArray(device.history) ? device.history : []
  };
}

function loadState() {
  if (stateLoaded) return;
  stateLoaded = true;
  try {
    if (!fs.existsSync(stateFile)) return;
    const saved = JSON.parse(fs.readFileSync(stateFile, "utf8"));
    if (saved.openApiConfig) {
      openApiConfig = {
        ...openApiConfig,
        ...saved.openApiConfig
      };
    }
    if (Array.isArray(saved.devicesList)) {
      const restoredDevices = saved.devicesList.map(normalizeDevice).filter(Boolean);
      if (restoredDevices.length) {
        devicesList = restoredDevices;
      }
    }
  } catch (error) {
    console.error("[SYS] Failed to load local state:", error.message);
  }
}

function saveState() {
  try {
    fs.mkdirSync(dataDir, { recursive: true });
    fs.writeFileSync(stateFile, JSON.stringify({
      openApiConfig,
      devicesList
    }, null, 2));
  } catch (error) {
    console.error("[SYS] Failed to save local state:", error.message);
  }
}

let devicesList = [createSensorDevice()];
loadState();
let sensorState = devicesList.find((device) => device.id === "Sensor");
if (!sensorState) {
  sensorState = createSensorDevice();
  devicesList.unshift(sensorState);
}
let iotOpenApiClient = null;

function updateDeviceStatus(device, nextStatus) {
  if (!device) return;
  const previousStatus = device.status;
  device.status = nextStatus;
  if ((previousStatus !== "offline" || !device.lastOfflineAt) && nextStatus === "offline") {
    device.lastOfflineAt = new Date().toISOString();
  }
}

function initOpenApiClient() {
  if (!openApiConfig.accessKeyId || !openApiConfig.accessKeySecret) {
    iotOpenApiClient = null;
    return false;
  }

  try {
    const IotClientClass = require("@alicloud/iot20180120").default;
    const { Config } = require("@alicloud/openapi-client");
    const openConfig = new Config({
      accessKeyId: openApiConfig.accessKeyId,
      accessKeySecret: openApiConfig.accessKeySecret
    });
    openConfig.endpoint = `iot.${openApiConfig.regionId}.aliyuncs.com`;
    iotOpenApiClient = new IotClientClass(openConfig);
    addApiLog("SYS", `阿里云 OpenAPI RAM 客户端加载成功。当前实例: ${devConfig.iotInstanceId}`);
    return true;
  } catch (error) {
    console.error("[SYS] Failed to load OpenAPI client:", error.message);
    addApiLog("API ERR", `OpenAPI 客户端初始化失败: ${error.message}`);
    iotOpenApiClient = null;
    return false;
  }
}

initOpenApiClient();

async function fetchRealDeviceData() {
  if (!iotOpenApiClient) return;

  try {
    const iot = require("@alicloud/iot20180120");

    const statusRequest = new iot.GetDeviceStatusRequest({
      iotInstanceId: devConfig.iotInstanceId,
      productKey: devConfig.productKey,
      deviceName: devConfig.deviceName
    });
    const statusResponse = await iotOpenApiClient.getDeviceStatus(statusRequest);
    const statusData = statusResponse.body.Data || statusResponse.body.data;
    const onlineStatus = statusData?.Status || statusData?.status || "OFFLINE";

    const propertyRequest = new iot.QueryDevicePropertyStatusRequest({
      iotInstanceId: devConfig.iotInstanceId,
      productKey: devConfig.productKey,
      deviceName: devConfig.deviceName
    });
    const propertyResponse = await iotOpenApiClient.queryDevicePropertyStatus(propertyRequest);
    const propData = propertyResponse.body.Data || propertyResponse.body.data;
    const propertyList = propData?.List?.PropertyStatusInfo || propData?.list?.PropertyStatusInfo || propData?.list?.propertyStatusInfo || [];

    if (!propertyList.length) {
      updateDeviceStatus(sensorState, "offline");
      sensorState.telemetryStale = true;
      sensorState.metrics.LEDSwitch0 = 0;
      sensorState.metrics.LEDSwitch1 = 0;
      addApiLog("API WARN", "拉取成功，但未读取到有效属性上报数据。");
      return;
    }

    const now = Date.now();
    let latestTelemetryTimestamp = 0;
    propertyList.forEach((item) => {
      const id = item.Identifier || item.identifier;
      const value = item.Value || item.value;
      const itemTime = Number(item.Time || item.time || 0);
      if (id === undefined || value === undefined) return;
      if (Number.isFinite(itemTime) && itemTime > latestTelemetryTimestamp) {
        latestTelemetryTimestamp = itemTime;
      }

      if (id === "LEDSwitch0") {
        const numericValue = parseInt(value, 10) || 0;
        sensorState.metrics.LEDSwitch0 = numericValue;
        return;
      }

      if (id === "LEDSwitch1") {
        const numericValue = parseInt(value, 10) || 0;
        sensorState.metrics.LEDSwitch1 = numericValue;
        return;
      }

      if (id === "Light_Out_Voltage") {
        const numericValue = parseFloat(value) || 0;
        if (now - lastCommandTimes.Light_Out_Voltage >= LOG_RETENTION_MS || Math.abs(numericValue - lastCommandValues.Light_Out_Voltage) < 0.01) {
          sensorState.metrics.Light_Out_Voltage = numericValue;
        }
        return;
      }

      if (id === "temperature") sensorState.metrics.temperature = parseFloat(value) || 0;
      if (id === "Humidity") sensorState.metrics.Humidity = parseFloat(value) || 0;
      if (id === "Yaw") sensorState.metrics.Yaw = parseFloat(value) || 0;
      if (id === "Roll") sensorState.metrics.Roll = parseFloat(value) || 0;
      if (id === "Pitch") sensorState.metrics.Pitch = parseFloat(value) || 0;
      if (id === "Light_ADC") sensorState.metrics.Light_ADC = parseInt(value, 10) || 0;
    });

    const effectiveTelemetryTimestamp = latestTelemetryTimestamp || now;
    const telemetryAgeSeconds = Math.max(0, Math.floor((now - effectiveTelemetryTimestamp) / 1000));
    sensorState.telemetryLastReportedAt = new Date(effectiveTelemetryTimestamp).toISOString();
    sensorState.telemetryAgeSeconds = telemetryAgeSeconds;
    sensorState.telemetryStale = telemetryAgeSeconds >= TELEMETRY_ONLINE_MAX_AGE_SECONDS;
    updateDeviceStatus(sensorState, onlineStatus === "ONLINE" && !sensorState.telemetryStale ? "online" : "offline");
    if (sensorState.status !== "online") {
      sensorState.metrics.LEDSwitch0 = 0;
      sensorState.metrics.LEDSwitch1 = 0;
    }

    addApiLog("API GET", `成功拉取物理 Sensor 最新真实属性数据（终端状态: ${onlineStatus === "ONLINE" ? "在线" : "离线"}）`);
  } catch (error) {
    addApiLog("API ERR", `云端数据拉取失败: ${error.message}`);
  }
}

function pushHistory(device, payload) {
  device.history.push({ time: new Date().toLocaleTimeString(), ...payload });
  if (device.history.length > 20) device.history.shift();
  device.lastUpdate = new Date().toISOString();
}

setInterval(async () => {
  for (const device of devicesList) {
    if (device.id === "Sensor") {
      if (iotOpenApiClient) {
        await fetchRealDeviceData();
      } else {
        updateDeviceStatus(device, "offline");
        device.telemetryLastReportedAt = null;
        device.telemetryAgeSeconds = 0;
        device.telemetryStale = true;
        device.metrics.LEDSwitch0 = 0;
        device.metrics.LEDSwitch1 = 0;
      }

      pushHistory(device, {
        temperature: device.metrics.temperature,
        Humidity: device.metrics.Humidity,
        Light_ADC: device.metrics.Light_ADC
      });
      continue;
    }

    if (device.type === "Generic") {
      updateDeviceStatus(device, "offline");
      device.telemetryLastReportedAt = null;
      device.telemetryAgeSeconds = 0;
      device.telemetryStale = true;
      continue;
    }

    updateDeviceStatus(device, "offline");
    device.telemetryLastReportedAt = null;
    device.telemetryAgeSeconds = 0;
    device.telemetryStale = true;
  }
  saveState();
}, 5000);

app.get("/api/status", (_req, res) => {
  res.json({
    mqttStatus: "disabled",
    openApiStatus: iotOpenApiClient ? "active" : "inactive",
    accessKeyId: openApiConfig.accessKeyId,
    hasAccessKeySecret: Boolean(openApiConfig.accessKeySecret),
    productKey: devConfig.productKey,
    deviceName: devConfig.deviceName,
    mqttHostUrl: devConfig.mqttHostUrl,
    regionId: openApiConfig.regionId
  });
});

app.post("/api/config", (req, res) => {
  const { accessKeyId, accessKeySecret, regionId } = req.body;
  openApiConfig.accessKeyId = accessKeyId || "";
  openApiConfig.accessKeySecret = openApiConfig.accessKeyId
    ? (accessKeySecret || openApiConfig.accessKeySecret || "")
    : "";
  openApiConfig.regionId = regionId || "cn-shanghai";

  const initialized = initOpenApiClient();
  saveState();
  res.json({
    success: true,
    openApiStatus: initialized ? "active" : "inactive",
    message: initialized
      ? "阿里云 OpenAPI RAM 客户端已成功联通，开始读取真实数据。"
      : "配置成功，OpenAPI 当前未激活，系统已切换到本地仿真模式。"
  });
});

app.post("/api/mqtt/reconnect", (_req, res) => {
  res.json({
    success: false,
    message: "当前系统使用 OpenAPI 模式，后端不再负责模拟 MQTT 终端连接。"
  });
});

app.get("/api/mqtt/logs", (_req, res) => {
  trimApiLogBuffer();
  res.json(
    [...apiLogBuffer]
      .sort((a, b) => b.timestamp - a.timestamp)
      .map((entry) => entry.line)
  );
});

app.get("/api/devices", (_req, res) => {
  res.json(devicesList);
});

app.post("/api/devices", (req, res) => {
  const { id, name, productKey, deviceName, type } = req.body;

  if (!id || !name) {
    res.status(400).json({ success: false, message: "设备 ID 和显示名称不能为空。" });
    return;
  }

  if (devicesList.some((device) => device.id === id)) {
    res.status(400).json({ success: false, message: `设备 ID "${id}" 已存在。` });
    return;
  }

  const isGeneric = (type || "Generic") === "Generic";
  const newDevice = {
    id,
    name,
    productKey: productKey || "",
    deviceName: deviceName || "",
    type: type || "Generic",
    status: "offline",
    lastOfflineAt: null,
    lastUpdate: new Date().toISOString(),
    metrics: isGeneric
      ? { Voltage: 220, Current: 1.2, Power: 0.264, Alarm: 0 }
      : { LEDSwitch0: 0, LEDSwitch1: 0, temperature: 25, Humidity: 50, Yaw: 0, Roll: 0, Pitch: 0, Light_ADC: 500, Light_Out_Voltage: 2.44 },
    history: Array.from({ length: 12 }, (_, index) => {
      const time = new Date(Date.now() - (12 - index) * 5000).toLocaleTimeString();
      return isGeneric
        ? { time, Voltage: +(218 + Math.random() * 4).toFixed(1), Current: +(1 + Math.random() * 0.5).toFixed(2), Power: +(0.22 + Math.random() * 0.1).toFixed(3) }
        : { time, temperature: +(24 + Math.random() * 2).toFixed(1), Humidity: +(48 + Math.random() * 4).toFixed(1), Light_ADC: +(450 + Math.floor(Math.random() * 50)) };
    })
  };

  devicesList.push(newDevice);
  saveState();
  addApiLog("SYS", `成功注册新设备 -> ID: ${id}, 名称: ${name}, 类型: ${newDevice.type}`);
  res.json({ success: true, devices: devicesList });
});

app.delete("/api/devices/:id", (req, res) => {
  const { id } = req.params;
  if (id === "Sensor") {
    res.status(400).json({ success: false, message: "核心物理 Sensor 设备受系统保护，不可删除。" });
    return;
  }

  const index = devicesList.findIndex((device) => device.id === id);
  if (index === -1) {
    res.status(404).json({ success: false, message: "未找到指定设备。" });
    return;
  }

  const [removed] = devicesList.splice(index, 1);
  saveState();
  addApiLog("SYS", `已删除设备 -> ID: ${id}, 名称: ${removed.name}`);
  res.json({ success: true, devices: devicesList });
});

app.put("/api/devices/:id", (req, res) => {
  const { id } = req.params;
  if (id === "Sensor") {
    res.status(400).json({ success: false, message: "核心物理 Sensor 设备受系统保护，不可修改。" });
    return;
  }

  const device = devicesList.find((item) => item.id === id);
  if (!device) {
    res.status(404).json({ success: false, message: "未找到指定设备。" });
    return;
  }

  const { name, productKey, deviceName, type } = req.body;
  if (!name) {
    res.status(400).json({ success: false, message: "显示名称不能为空。" });
    return;
  }

  device.name = name;
  device.productKey = productKey || "";
  device.deviceName = deviceName || "";
  device.type = type || device.type || "Sensor";
  device.lastUpdate = new Date().toISOString();
  saveState();
  addApiLog("SYS", `已更新设备记录 -> ID: ${id}, 名称: ${device.name}`);
  res.json({ success: true, devices: devicesList });
});

app.get("/api/devices/:id/history", (req, res) => {
  const device = devicesList.find((item) => item.id === req.params.id);
  if (!device) {
    res.status(404).json({ error: "Device not found" });
    return;
  }

  res.json(device.history);
});

app.post("/api/devices/:id/command", async (req, res) => {
  const device = devicesList.find((item) => item.id === req.params.id);
  const { command, value } = req.body;

  if (!device) {
    res.status(404).json({ success: false, message: "设备未找到。" });
    return;
  }

  const logs = [];
  const pushLog = (type, message) => {
    logs.push(`[${new Date().toLocaleTimeString()}] [${type}] ${message}`);
  };

  pushLog("SYS", "拦截网页端操作指令");
  pushLog("CMD", `目标设备: ${device.name} (${device.id})`);
  pushLog("CMD", `动作: "${command}" 载荷: ${JSON.stringify(value)}`);

  if (device.id !== "Sensor") {
    pushLog("SYS", "通用监测设备不具备实际下行控制通道，指令已记录为模拟事件。");
    addApiLog("API SET", `向通用监测设备 ${device.name} 发送指令: ${command} = ${value}`);
    res.json({ success: true, logs, device });
    return;
  }

  let itemKey = command;
  let valueToSet = value;

  if (command === "LEDSwitch0") {
    device.metrics.LEDSwitch0 = value ? 1 : 0;
    valueToSet = value ? 1 : 0;
    lastCommandTimes.LEDSwitch0 = Date.now();
    lastCommandValues.LEDSwitch0 = valueToSet;
  } else if (command === "LEDSwitch1") {
    device.metrics.LEDSwitch1 = value ? 1 : 0;
    valueToSet = value ? 1 : 0;
    lastCommandTimes.LEDSwitch1 = Date.now();
    lastCommandValues.LEDSwitch1 = valueToSet;
  } else if (command === "Light_Out_Voltage") {
    device.metrics.Light_Out_Voltage = parseFloat(value);
    valueToSet = parseFloat(value);
    lastCommandTimes.Light_Out_Voltage = Date.now();
    lastCommandValues.Light_Out_Voltage = valueToSet;
  }

  pushLog("LOCAL", "本地状态缓存已预更新。");

  if (iotOpenApiClient) {
    try {
      const iot = require("@alicloud/iot20180120");
      const request = new iot.SetDevicePropertyRequest({
        iotInstanceId: devConfig.iotInstanceId,
        productKey: devConfig.productKey,
        deviceName: devConfig.deviceName,
        items: JSON.stringify({ [itemKey]: valueToSet })
      });

      pushLog("API", `调用 SetDeviceProperty (${itemKey} = ${valueToSet})`);
      const response = await iotOpenApiClient.setDeviceProperty(request);

      if (response?.body?.success) {
        pushLog("RX", `阿里云 OpenAPI 下发成功，RequestId: ${response.body.requestId || response.body.RequestId}`);
        pushLog("CLOUD", "云端属性设置命令已成功派发。");
        addApiLog("API SET", `成功向物理硬件下发属性设置指令: ${itemKey} = ${valueToSet}`);
      } else {
        const errorMessage = response?.body?.errorMessage || "未知云端错误";
        pushLog("ERR", `阿里云 OpenAPI 调用失败: ${errorMessage}`);
        addApiLog("API ERR", `下发属性 ${itemKey} 失败: ${errorMessage}`);
      }
    } catch (error) {
      pushLog("ERR", `阿里云 OpenAPI 调用异常: ${error.message}`);
      addApiLog("API ERR", `下发属性 ${itemKey} 发生异常: ${error.message}`);
    }
  } else {
    pushLog("WARN", "当前处于本地仿真模式，指令仅在页面本地生效，不会下发到云端。");
  }

  if (command === "reboot") {
    updateDeviceStatus(device, "offline");
    saveState();
    pushLog("WARN", "设备正在重启，短时间内会显示离线。");
    setTimeout(() => {
      device.status = "online";
      addApiLog("SYS", "设备重启模拟结束，已恢复在线状态。");
    }, 6000);
  }

  res.json({ success: true, logs, device });
});

let httpServer = null;

function startServer(port = PORT) {
  if (httpServer) {
    return httpServer;
  }

  httpServer = app.listen(port, () => {
    console.log("=======================================================");
    console.log(" Aliyun IoT Dashboard server is running");
    console.log(` Local server: http://localhost:${port}`);
    console.log(` Bound device: ${devConfig.productKey} / ${devConfig.deviceName}`);
    console.log("=======================================================");
  });

  return httpServer;
}

if (require.main === module) {
  startServer();
}

module.exports = {
  app,
  startServer
};
