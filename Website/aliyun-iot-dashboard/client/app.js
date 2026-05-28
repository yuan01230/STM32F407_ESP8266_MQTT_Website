let devices = [];
let selectedDeviceId = "Sensor";
let telemetryCharts = {
  temperature: null,
  humidity: null,
  generic: null
};
let currentChartType = null;
const API_BASE_URL = window.location.origin;
let savedOpenApiSecretExists = false;
let editingDeviceId = null;

let lastCommandTimes = {
  LEDSwitch0: 0,
  LEDSwitch1: 0,
  Light_Out_Voltage: 0
};

document.addEventListener("DOMContentLoaded", () => {
  initClock();
  setupEventListeners();
  void fetchSystemStatus();
  void fetchDevices(true);

  setInterval(() => {
    void fetchDevices(false);
  }, 3000);
});

function initClock() {
  const clock = document.getElementById("clock-time");
  if (!clock) return;

  const render = () => {
    clock.textContent = new Date().toTimeString().split(" ")[0];
  };

  render();
  setInterval(render, 1000);
}

function addConsoleLog(type, text) {
  console.debug(`[${type}] ${text}`);
}

async function pullLogs() {
  // Terminal log panel has been removed from the UI.
}

async function fetchSystemStatus() {
  try {
    const response = await fetch(`${API_BASE_URL}/api/status`);
    if (!response.ok) return;

    const status = await response.json();
    const dot = document.getElementById("status-indicator-dot");
    const label = document.getElementById("status-indicator-label");
    const desc = document.getElementById("status-indicator-desc");
    const regionSelect = document.getElementById("input-regionId");
    const accessKeyIdInput = document.getElementById("input-accessKeyId");
    const accessKeySecretInput = document.getElementById("input-accessKeySecret");

    if (regionSelect) {
      regionSelect.value = status.regionId || "cn-shanghai";
    }
    if (accessKeyIdInput) {
      accessKeyIdInput.value = status.accessKeyId || "";
    }
    savedOpenApiSecretExists = Boolean(status.hasAccessKeySecret);
    if (accessKeySecretInput) {
      accessKeySecretInput.required = !savedOpenApiSecretExists;
      accessKeySecretInput.value = "";
      accessKeySecretInput.placeholder = savedOpenApiSecretExists
        ? "已保存密钥；不修改可留空"
        : "输入 32 位阿里云密钥凭证密钥";
    }

    if (!dot) return;
    dot.className = "status-dot pulsing";

    if (status.openApiStatus === "active") {
      dot.classList.add("cloud-mode");
      if (label) label.textContent = "云端通道: 已连接";
      if (desc) desc.textContent = "正在通过 OpenAPI 读取真实设备状态";
      return;
    }

    dot.classList.add("mock-mode");
    if (label) label.textContent = "本地仿真模式";
    if (desc) desc.textContent = "当前未配置 RAM OpenAPI 凭证";
  } catch (error) {
    console.error("Failed to retrieve status:", error);
  }
}

function ensureGenericOverlays() {
  const instrumentCard = document.querySelector(".cockpit-instrument-card");
  const controlCard = document.querySelector(".cockpit-control-card");

  if (instrumentCard && !instrumentCard.querySelector(".device-generic-overlay")) {
    const overlay = document.createElement("div");
    overlay.className = "device-generic-overlay";
    overlay.innerHTML = `
      <div class="overlay-icon"><i class="fa-solid fa-satellite-dish"></i></div>
      <div class="overlay-title">3D 姿态仪不可用</div>
      <div class="overlay-desc">当前设备类型为通用监测设备，不具备三维姿态传感器。</div>
    `;
    instrumentCard.appendChild(overlay);
  }

  if (controlCard && !controlCard.querySelector(".device-generic-overlay")) {
    const overlay = document.createElement("div");
    overlay.className = "device-generic-overlay";
    overlay.innerHTML = `
      <div class="overlay-icon"><i class="fa-solid fa-ban"></i></div>
      <div class="overlay-title">下行控制不可用</div>
      <div class="overlay-desc">该设备当前仅用于监测展示，未配置实际控制通道。</div>
    `;
    controlCard.appendChild(overlay);
  }
}

function renderSidebarDevicesList() {
  const container = document.getElementById("sidebar-devices-list");
  if (!container) return;

  container.innerHTML = devices.filter((device) => device.id === "Sensor").map((device) => {
    const activeClass = device.id === selectedDeviceId ? "active" : "";
    const onlineClass = device.status === "online" ? "online" : "offline";
    const statusText = device.status === "online" ? "在线" : "离线";

    return `
      <a href="#" class="sidebar-device-item ${activeClass}" data-device-id="${device.id}">
        <div class="sidebar-device-meta">
          <div class="sidebar-device-header">
            <span class="sidebar-device-name" title="${device.name}">${device.name}</span>
          </div>
          <div class="sidebar-device-status-row">
            <span class="sidebar-device-status-dot ${onlineClass}"></span>
            <span class="sidebar-device-status-lbl">${statusText}</span>
          </div>
        </div>
        <i class="fa-solid fa-chevron-right chevron-icon" style="font-size: 0.65rem; opacity: 0.5;"></i>
      </a>
    `;
  }).join("");

  container.querySelectorAll(".sidebar-device-item").forEach((item) => {
    item.addEventListener("click", (event) => {
      event.preventDefault();
      selectDevice(item.getAttribute("data-device-id"));
    });
  });
}

function renderHeaderDeviceSelect() {
  const select = document.getElementById("device-select");
  if (!select) return;

  select.innerHTML = devices.map((device) => {
    return `<option value="${device.id}" ${device.id === selectedDeviceId ? "selected" : ""}>${device.name}</option>`;
  }).join("");
}

function renderModalDevicesList() {
  const container = document.getElementById("device-management-list");
  if (!container) return;

  const managedDevices = devices.filter((device) => device.id !== "Sensor");

  if (!managedDevices.length) {
    container.innerHTML = `
      <div class="device-manager-empty">
        暂无已添加的阿里云终端记录。
      </div>
    `;
    return;
  }

  container.innerHTML = managedDevices.map((device) => {
    const isSensor = device.id === "Sensor";
    const badgeClass = isSensor ? "badge-sensor" : "badge-generic";
    const badgeText = isSensor ? "阿里云终端" : "通用终端";
    const statusText = device.status === "online" ? "在线" : "离线";
    const statusClass = device.status === "online" ? "online" : "offline";
    const offlineTime = device.lastOfflineAt
      ? new Date(device.lastOfflineAt).toLocaleString()
      : "暂无记录";
    const updateTime = device.telemetryLastReportedAt
      ? new Date(device.telemetryLastReportedAt).toLocaleString()
      : "暂无记录";
    const timeLabel = device.status === "online" ? "数据更新时间" : "离线时间";
    const timeValue = device.status === "online" ? updateTime : offlineTime;

    return `
      <div class="device-manager-item">
        <div class="device-manager-info">
          <div class="device-manager-name">
            ${device.name}
            <span class="device-manager-badge ${badgeClass}">${badgeText}</span>
            <span class="device-manager-status ${statusClass}">
              <span class="sidebar-device-status-dot ${statusClass}"></span>${statusText}
            </span>
          </div>
          <div class="device-manager-details">
            ID: ${device.id} | ProductKey: ${device.productKey || "无"} | DeviceName: ${device.deviceName || "无"}
          </div>
          <div class="device-manager-details">
            ${timeLabel}: ${timeValue}
          </div>
          <div class="device-manager-actions">
            <button class="btn-edit-device" data-device-id="${device.id}">
              <i class="fa-solid fa-pen"></i> 修改
            </button>
            <button class="btn-delete-device" data-device-id="${device.id}" title="删除终端记录">
              <i class="fa-solid fa-trash-can"></i> 删除
            </button>
          </div>
        </div>
      </div>
    `;
  }).join("");

  container.querySelectorAll(".btn-edit-device").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      openDeviceForm(button.getAttribute("data-device-id"));
    });
  });

  container.querySelectorAll(".btn-delete-device").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      void deleteDevice(button.getAttribute("data-device-id"));
    });
  });
}

function showPage(pageName) {
  const dashboardPage = document.getElementById("dashboard-page");
  const devicePage = document.getElementById("device-management-page");
  const showDevices = pageName === "devices";

  dashboardPage?.classList.toggle("hidden", showDevices);
  devicePage?.classList.toggle("hidden", !showDevices);

  document.querySelectorAll(".nav-item").forEach((item) => {
    const isActive = showDevices
      ? item.id === "btn-open-devices-sidebar"
      : item.dataset.target === "dashboard";
    item.classList.toggle("active", isActive);
  });
}

function openDeviceForm(deviceId = null) {
  const devicesModal = document.getElementById("devices-modal");
  const form = document.getElementById("add-device-form");
  const title = document.getElementById("device-form-title");
  const saveButton = document.getElementById("btn-save-device-record");
  const idInput = document.getElementById("input-device-id");
  const nameInput = document.getElementById("input-device-name");
  const typeInput = document.getElementById("input-device-type");
  const productKeyInput = document.getElementById("input-device-pk");
  const deviceNameInput = document.getElementById("input-device-dn");
  const originalIdInput = document.getElementById("input-device-original-id");
  const device = devices.find((item) => item.id === deviceId);

  editingDeviceId = device?.id || null;
  form?.reset();
  if (originalIdInput) originalIdInput.value = editingDeviceId || "";
  if (title) title.textContent = editingDeviceId ? "修改阿里云终端" : "添加阿里云终端";
  if (saveButton) saveButton.innerHTML = editingDeviceId
    ? '<i class="fa-solid fa-floppy-disk"></i> 保存修改'
    : '<i class="fa-solid fa-plus"></i> 保存终端记录';
  if (idInput) {
    idInput.value = device?.id || "";
    idInput.readOnly = Boolean(editingDeviceId);
  }
  if (nameInput) nameInput.value = device?.name || "";
  if (typeInput) typeInput.value = device?.type || "Sensor";
  if (productKeyInput) productKeyInput.value = device?.productKey || "";
  if (deviceNameInput) deviceNameInput.value = device?.deviceName || "";
  devicesModal?.classList.add("active");
}

function closeDeviceForm() {
  const devicesModal = document.getElementById("devices-modal");
  const idInput = document.getElementById("input-device-id");
  editingDeviceId = null;
  if (idInput) idInput.readOnly = false;
  devicesModal?.classList.remove("active");
}

function selectDevice(deviceId) {
  selectedDeviceId = deviceId || "Sensor";
  addConsoleLog("sys", `切换查看终端视图 -> 设备 ID: ${selectedDeviceId}`);
  void fetchDevices(false);
}

function formatTelemetryAge(seconds) {
  if (!Number.isFinite(seconds) || seconds <= 0) return "刚刚";
  if (seconds < 60) return `${seconds} 秒前`;

  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes} 分钟前`;

  const hours = Math.floor(minutes / 60);
  return `${hours} 小时前`;
}

function renderSensorBanner(device) {
  const banner = document.getElementById("cloud-sync-banner");
  if (!banner) return;

  const title = banner.querySelector("h3");
  const content = banner.querySelector("p");
  const actionButton = document.getElementById("btn-banner-config-trigger");
  const telemetryAgeText = formatTelemetryAge(device.telemetryAgeSeconds || 0);

  if (device.status === "online") {
    if (device.telemetryStale) {
      if (title) title.textContent = "云端已连接，但最近没有新的遥测上报";
      if (content) {
        content.innerHTML = `阿里云 OpenAPI 已接通，<strong>Sensor</strong> 当前仍在线，但最近一次有效遥测上报停留在 <strong>${telemetryAgeText}</strong>。页面显示的是最后一次收到的真实数据，这说明当前是设备上报停滞，不是页面故障。`;
      }
    } else {
      if (title) title.textContent = "云端已连接，当前读取真实设备数据";
      if (content) {
        content.innerHTML = "当前系统已接入阿里云 OpenAPI，面板显示的是 <strong>Sensor</strong> 设备的真实状态与数据。";
      }
    }
    if (actionButton) actionButton.textContent = "更新 OpenAPI 配置";
    banner.classList.remove("hidden");
    return;
  }

  if (title) title.textContent = "云端已连接，但真实设备当前离线";
  if (content) {
    content.innerHTML = "阿里云 OpenAPI 已经联通，但 <strong>Sensor</strong> 当前返回的是离线状态，因此页面会显示离线设备而不是本地仿真数据。这说明是设备状态问题，不是页面故障。";
  }
  if (actionButton) actionButton.textContent = "查看 OpenAPI 配置";
  banner.classList.remove("hidden");
}

function updateActiveDeviceStatus(device) {
  const dot = document.getElementById("status-indicator-dot");
  const label = document.getElementById("status-indicator-label");
  const desc = document.getElementById("status-indicator-desc");
  const headerTitle = document.getElementById("main-header-title");
  const headerSubtitle = document.querySelector(".header-subtitle");
  const banner = document.getElementById("cloud-sync-banner");

  if (headerTitle) {
    headerTitle.textContent = `${device.name} 控制舱`;
  }

  if (headerSubtitle) {
    if (device.id === "Sensor") {
      headerSubtitle.textContent = "实时监控物理终端 Sensor 的真实遥测状态，并支持通过物模型下发控制指令";
    } else {
      headerSubtitle.textContent = `实时监控通用设备 ${device.name} 的运行状态与功率数据`;
    }
  }

  if (!dot) return;
  dot.className = "status-dot pulsing";

  if (device.status === "online") {
    dot.classList.add("cloud-mode");
    if (label) label.textContent = "设备状态: 在线";
    if (desc) {
      desc.textContent = device.id === "Sensor"
        ? (device.telemetryStale
            ? `OpenAPI 已连接，但最近 ${formatTelemetryAge(device.telemetryAgeSeconds || 0)} 没有新的遥测数据`
            : "OpenAPI 已连接，当前展示真实设备在线数据")
        : "设备状态正常，正在持续刷新实时数据";
    }
  } else {
    dot.classList.add("mock-mode");
    if (label) label.textContent = "设备状态: 离线";
    if (desc) {
      desc.textContent = device.id === "Sensor"
        ? "OpenAPI 已连接，但真实设备当前离线"
        : "当前未收到该设备的在线状态或有效上报";
    }
  }

  if (device.id === "Sensor") {
    renderSensorBanner(device);
  } else {
    banner?.classList.add("hidden");
  }
}

function updateTelemetryCards(device) {
  const titles = {
    temp: document.querySelector("#card-temp .stat-title"),
    hum: document.querySelector("#card-hum .stat-title"),
    adc: document.querySelector("#card-adc .stat-title"),
    vol: document.querySelector("#card-vol .stat-title")
  };

  const values = {
    temp: document.getElementById("main-val-temp"),
    hum: document.getElementById("main-val-hum"),
    adc: document.getElementById("main-val-adc"),
    vol: document.getElementById("main-val-vol")
  };

  if (device.type === "Sensor") {
    if (titles.temp) titles.temp.textContent = "实时温度";
    if (titles.hum) titles.hum.textContent = "环境湿度";
    if (titles.adc) titles.adc.textContent = "光敏电阻 ADC";
    if (titles.vol) titles.vol.textContent = "恒定输出电压";

    if (device.status === "online") {
      if (values.temp) values.temp.innerHTML = `${device.metrics.temperature.toFixed(1)} <span class="stat-unit">°C</span>`;
      if (values.hum) values.hum.innerHTML = `${device.metrics.Humidity.toFixed(1)} <span class="stat-unit">%</span>`;
      if (values.adc) values.adc.innerHTML = `${device.metrics.Light_ADC} <span class="stat-unit">ADC</span>`;
      if (values.vol) values.vol.innerHTML = `${device.metrics.Light_Out_Voltage.toFixed(2)} <span class="stat-unit">V</span>`;
    } else {
      if (values.temp) values.temp.innerHTML = `-- <span class="stat-unit">°C</span>`;
      if (values.hum) values.hum.innerHTML = `-- <span class="stat-unit">%</span>`;
      if (values.adc) values.adc.innerHTML = `-- <span class="stat-unit">ADC</span>`;
      if (values.vol) values.vol.innerHTML = `-- <span class="stat-unit">V</span>`;
    }
    return;
  }

  if (titles.temp) titles.temp.textContent = "电网电压";
  if (titles.hum) titles.hum.textContent = "回路电流";
  if (titles.adc) titles.adc.textContent = "有功功率";
  if (titles.vol) titles.vol.textContent = "告警级别";

  if (values.temp) values.temp.innerHTML = `${(device.metrics.Voltage || 0).toFixed(1)} <span class="stat-unit">V</span>`;
  if (values.hum) values.hum.innerHTML = `${(device.metrics.Current || 0).toFixed(2)} <span class="stat-unit">A</span>`;
  if (values.adc) values.adc.innerHTML = `${(device.metrics.Power || 0).toFixed(3)} <span class="stat-unit">kW</span>`;

  const alarm = device.metrics.Alarm || 0;
  let alarmText = "正常";
  let alarmColor = "var(--neon-green)";
  if (alarm === 1) {
    alarmText = "过压告警";
    alarmColor = "var(--neon-orange)";
  } else if (alarm === 2) {
    alarmText = "过流告警";
    alarmColor = "var(--neon-red)";
  }
  if (values.vol) values.vol.innerHTML = `<span style="color: ${alarmColor}">${alarmText}</span>`;
}

function updateCockpitPanels(device) {
  const instrumentOverlay = document.querySelector(".cockpit-instrument-card .device-generic-overlay");
  const controlOverlay = document.querySelector(".cockpit-control-card .device-generic-overlay");

  if (device.type !== "Sensor") {
    instrumentOverlay?.classList.add("show");
    controlOverlay?.classList.add("show");
    return;
  }

  instrumentOverlay?.classList.remove("show");
  controlOverlay?.classList.remove("show");

  const compassDial = document.getElementById("yaw-compass-dial");
  const yawDegVal = document.getElementById("yaw-deg-val");
  const horizonBall = document.getElementById("attitude-sphere-ball");
  const rollDegVal = document.getElementById("roll-deg-val");
  const pitchDegVal = document.getElementById("pitch-deg-val");
  const checkLed0 = document.getElementById("ctrl-Sensor-led0");
  const checkLed1 = document.getElementById("ctrl-Sensor-led1");
  const sliderVol = document.getElementById("ctrl-Sensor-vol");
  const sliderVolVal = document.getElementById("ctrl-Sensor-vol-val");

  if (device.status !== "online") {
    if (compassDial) compassDial.style.transform = "rotate(0deg)";
    if (yawDegVal) yawDegVal.textContent = "--°";
    if (horizonBall) horizonBall.style.transform = "rotate(0deg) translateY(0px)";
    if (rollDegVal) rollDegVal.textContent = "--°";
    if (pitchDegVal) pitchDegVal.textContent = "--°";
    if (checkLed0) checkLed0.checked = false;
    if (checkLed1) checkLed1.checked = false;
    return;
  }

  if (compassDial) compassDial.style.transform = `rotate(${-device.metrics.Yaw}deg)`;
  if (yawDegVal) yawDegVal.textContent = `${device.metrics.Yaw.toFixed(1)}°`;
  if (horizonBall) {
    const translateY = device.metrics.Pitch * 0.7;
    const rotateDeg = -device.metrics.Roll;
    horizonBall.style.transform = `rotate(${rotateDeg}deg) translateY(${translateY}px)`;
  }
  if (rollDegVal) rollDegVal.textContent = `${device.metrics.Roll.toFixed(1)}°`;
  if (pitchDegVal) pitchDegVal.textContent = `${device.metrics.Pitch.toFixed(1)}°`;

  if (checkLed0) {
    checkLed0.checked = device.metrics.LEDSwitch0 === 1;
  }
  if (checkLed1) {
    checkLed1.checked = device.metrics.LEDSwitch1 === 1;
  }
  const now = Date.now();
  if (sliderVol && now - lastCommandTimes.Light_Out_Voltage > 15000) {
    sliderVol.value = String(device.metrics.Light_Out_Voltage);
    if (sliderVolVal) sliderVolVal.textContent = `${device.metrics.Light_Out_Voltage.toFixed(2)}V`;
  }
}

async function fetchDevices(initialLoad = false) {
  try {
    const response = await fetch(`${API_BASE_URL}/api/devices?t=${Date.now()}`);
    if (!response.ok) throw new Error("API server down");

    devices = (await response.json()).map((device) => {
      if (device.status !== "online" && device.metrics) {
        return {
          ...device,
          metrics: {
            ...device.metrics,
            LEDSwitch0: 0,
            LEDSwitch1: 0
          }
        };
      }
      return device;
    });
    ensureGenericOverlays();
    renderSidebarDevicesList();
    renderHeaderDeviceSelect();
    renderModalDevicesList();

    let activeDevice = devices.find((device) => device.id === selectedDeviceId);
    if (!activeDevice && devices.length > 0) {
      activeDevice = devices[0];
      selectedDeviceId = activeDevice.id;
    }
    if (!activeDevice) return;

    updateActiveDeviceStatus(activeDevice);
    updateTelemetryCards(activeDevice);
    updateCockpitPanels(activeDevice);

    if (initialLoad || currentChartType !== activeDevice.type) {
      setupTelemetryChart(activeDevice);
    } else {
      updateTelemetryChartData(activeDevice);
    }
  } catch (error) {
    console.error("Error fetching device data:", error);
  }
}

function setupTelemetryChart(device) {
  telemetryCharts.temperature?.destroy();
  telemetryCharts.humidity?.destroy();
  telemetryCharts.generic?.destroy();
  telemetryCharts.temperature = null;
  telemetryCharts.humidity = null;
  telemetryCharts.generic = null;
  currentChartType = device.type;

  const labels = device.history.map((item) => item.time);
  Chart.defaults.color = "#9ca3af";
  Chart.defaults.font.family = "Outfit";

  const chartTitle = document.querySelector(".cockpit-chart-card h2");
  const sensorChartGrid = document.getElementById("sensor-chart-grid");
  const genericChartContainer = document.getElementById("generic-chart-container");

  if (device.type === "Sensor") {
    sensorChartGrid?.classList.remove("hidden");
    genericChartContainer?.classList.add("hidden");

    const temperatureCanvas = document.getElementById("temperatureChart");
    const humidityCanvas = document.getElementById("humidityChart");
    const temperatureCtx = temperatureCanvas?.getContext("2d");
    const humidityCtx = humidityCanvas?.getContext("2d");
    if (!temperatureCtx || !humidityCtx) return;

    const commonOptions = {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: "index", intersect: false },
      plugins: { legend: { display: false } },
      scales: {
        x: { grid: { color: "rgba(255, 255, 255, 0.03)" }, ticks: { font: { size: 9 } } },
        y: { type: "linear", display: true, position: "left", grid: { color: "rgba(255, 255, 255, 0.03)" }, ticks: { font: { size: 9 } } }
      }
    };

    telemetryCharts.temperature = new Chart(temperatureCtx, {
      type: "line",
      data: {
        labels,
        datasets: [
          { label: "温度 (°C)", data: device.history.map((item) => item.temperature), borderColor: "#05f3a0", backgroundColor: "rgba(5, 243, 160, 0.08)", tension: 0.35, fill: true }
        ]
      },
      options: commonOptions
    });

    telemetryCharts.humidity = new Chart(humidityCtx, {
      type: "line",
      data: {
        labels,
        datasets: [
          { label: "湿度 (%)", data: device.history.map((item) => item.Humidity), borderColor: "#00f2fe", backgroundColor: "rgba(0, 242, 254, 0.08)", tension: 0.35, fill: true }
        ]
      },
      options: commonOptions
    });

    if (chartTitle) {
      chartTitle.innerHTML = `<i class="fa-solid fa-chart-area"></i> 物理设备核心数据历史趋势`;
    }
    return;
  }

  sensorChartGrid?.classList.add("hidden");
  genericChartContainer?.classList.remove("hidden");

  const canvas = document.getElementById("telemetryChart");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  telemetryCharts.generic = new Chart(ctx, {
    type: "line",
    data: {
      labels,
      datasets: [
        { label: "电压 (V)", data: device.history.map((item) => item.Voltage), borderColor: "#ff6a00", backgroundColor: "rgba(255, 106, 0, 0.05)", tension: 0.35, fill: true, yAxisID: "y" },
        { label: "电流 (A)", data: device.history.map((item) => item.Current), borderColor: "#00f2fe", backgroundColor: "rgba(0, 242, 254, 0.05)", tension: 0.35, fill: true, yAxisID: "y1" },
        { label: "功率 (kW)", data: device.history.map((item) => item.Power), borderColor: "#b927fc", backgroundColor: "rgba(185, 39, 252, 0.02)", tension: 0.35, yAxisID: "y1" }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: "index", intersect: false },
      plugins: { legend: { position: "top", labels: { font: { size: 10 } } } },
      scales: {
        x: { grid: { color: "rgba(255, 255, 255, 0.03)" }, ticks: { font: { size: 9 } } },
        y: { type: "linear", display: true, position: "left", grid: { color: "rgba(255, 255, 255, 0.03)" }, ticks: { font: { size: 9 } } },
        y1: { type: "linear", display: true, position: "right", grid: { drawOnChartArea: false }, ticks: { font: { size: 9 } } }
      }
    }
  });

  if (chartTitle) {
    chartTitle.innerHTML = `<i class="fa-solid fa-chart-area"></i> 通用电能设备历史趋势 (电压 / 电流 / 功率)`;
  }
}

function updateTelemetryChartData(device) {
  const labels = device.history.map((item) => item.time);

  if (device.type === "Sensor") {
    if (!telemetryCharts.temperature || !telemetryCharts.humidity) return;
    telemetryCharts.temperature.data.labels = labels;
    telemetryCharts.temperature.data.datasets[0].data = device.history.map((item) => item.temperature);
    telemetryCharts.humidity.data.labels = labels;
    telemetryCharts.humidity.data.datasets[0].data = device.history.map((item) => item.Humidity);
    telemetryCharts.temperature.update("none");
    telemetryCharts.humidity.update("none");
    return;
  }

  if (!telemetryCharts.generic) return;
  telemetryCharts.generic.data.labels = labels;
  telemetryCharts.generic.data.datasets[0].data = device.history.map((item) => item.Voltage);
  telemetryCharts.generic.data.datasets[1].data = device.history.map((item) => item.Current);
  telemetryCharts.generic.data.datasets[2].data = device.history.map((item) => item.Power);
  telemetryCharts.generic.update("none");
}

async function dispatchDeviceCommand(commandName, commandValue) {
  const device = devices.find((item) => item.id === selectedDeviceId) || { name: "未知设备", id: selectedDeviceId };
  if (commandName in lastCommandTimes) {
    lastCommandTimes[commandName] = Date.now();
  }

  addConsoleLog("cmd", `本地指令触发 -> 目标: ${device.name}, 属性: ${commandName}, 值: ${JSON.stringify(commandValue)}`);

  try {
    const response = await fetch(`${API_BASE_URL}/api/devices/${selectedDeviceId}/command`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ command: commandName, value: commandValue })
    });
    const result = await response.json();

    if (!result.success) {
      addConsoleLog("err", `指令执行失败：${result.message || "网关无响应"}`);
      return;
    }

    if (Array.isArray(result.logs)) {
      result.logs.forEach((log) => {
        if (log.includes("[ERR]")) addConsoleLog("err", log.replace(/.*\[ERR\]\s*/, ""));
        else if (log.includes("[WARN]")) addConsoleLog("warn", log.replace(/.*\[WARN\]\s*/, ""));
        else if (log.includes("[RX]")) addConsoleLog("rx", log.replace(/.*\[RX\]\s*/, ""));
        else if (log.includes("[TX]")) addConsoleLog("tx", log.replace(/.*\[TX\]\s*/, ""));
        else addConsoleLog("sys", log.replace(/.*\[(SYS|CMD|LOCAL|CLOUD|API)\]\s*/, ""));
      });
    }

    setTimeout(() => void fetchDevices(false), 300);
  } catch (error) {
    addConsoleLog("err", "网络连接错误：无法访问控制接口。");
    console.error(error);
  }
}

async function addDevice(id, name, type, productKey, deviceName) {
  try {
    const response = await fetch(`${API_BASE_URL}/api/devices`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id, name, type, productKey, deviceName })
    });
    const result = await response.json();

    if (!result.success) {
      addConsoleLog("err", `注册设备失败：${result.message}`);
      alert(`注册设备失败: ${result.message}`);
      return false;
    }

    addConsoleLog("cmd", `设备 ${name} 注册成功并已上线。`);
    selectedDeviceId = id;
    await fetchDevices(true);
    return true;
  } catch (error) {
    addConsoleLog("err", "通信故障：无法连接后端注册接口。");
    console.error(error);
    return false;
  }
}

async function updateDevice(id, payload) {
  try {
    const response = await fetch(`${API_BASE_URL}/api/devices/${id}`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    const result = await response.json();

    if (!result.success) {
      addConsoleLog("err", `修改终端失败：${result.message}`);
      alert(`修改终端失败: ${result.message}`);
      return false;
    }

    addConsoleLog("cmd", `终端 ${id} 已更新。`);
    await fetchDevices(true);
    return true;
  } catch (error) {
    addConsoleLog("err", "通信故障：无法连接后端修改接口。");
    console.error(error);
    return false;
  }
}

async function deleteDevice(id) {
  if (id === "Sensor") {
    alert("核心 Sensor 设备受保护，不能删除。");
    return;
  }

  if (!confirm(`确认删除设备 "${id}" 吗？`)) return;

  try {
    const response = await fetch(`${API_BASE_URL}/api/devices/${id}`, {
      method: "DELETE"
    });
    const result = await response.json();

    if (!result.success) {
      addConsoleLog("err", `删除失败：${result.message}`);
      alert(`删除失败: ${result.message}`);
      return;
    }

    addConsoleLog("cmd", `设备 ${id} 已删除。`);
    if (selectedDeviceId === id) selectedDeviceId = "Sensor";
    await fetchDevices(true);
  } catch (error) {
    addConsoleLog("err", "通信故障：无法连接后端删除接口。");
    console.error(error);
  }
}

function setupEventListeners() {
  document.getElementById("ctrl-Sensor-led0")?.addEventListener("change", (event) => {
    void dispatchDeviceCommand("LEDSwitch0", event.target.checked);
  });

  document.getElementById("ctrl-Sensor-led1")?.addEventListener("change", (event) => {
    void dispatchDeviceCommand("LEDSwitch1", event.target.checked);
  });

  const sliderVol = document.getElementById("ctrl-Sensor-vol");
  sliderVol?.addEventListener("input", (event) => {
    const label = document.getElementById("ctrl-Sensor-vol-val");
    if (label) label.textContent = `${parseFloat(event.target.value).toFixed(2)}V`;
  });
  sliderVol?.addEventListener("change", (event) => {
    void dispatchDeviceCommand("Light_Out_Voltage", parseFloat(event.target.value));
  });

  document.getElementById("btn-reboot-device")?.addEventListener("click", () => {
    if (confirm("确认向设备发送重启命令吗？")) {
      void dispatchDeviceCommand("reboot", true);
    }
  });

  const configModal = document.getElementById("config-modal");
  const openConfig = async () => {
    await fetchSystemStatus();
    configModal?.classList.add("active");
  };
  const closeConfig = () => configModal?.classList.remove("active");

  document.getElementById("btn-config-trigger")?.addEventListener("click", openConfig);
  document.getElementById("btn-open-config-sidebar")?.addEventListener("click", (event) => {
    event.preventDefault();
    openConfig();
  });
  document.getElementById("btn-banner-config-trigger")?.addEventListener("click", openConfig);
  document.getElementById("btn-close-config-drawer")?.addEventListener("click", closeConfig);

  configModal?.addEventListener("click", (event) => {
    if (event.target === configModal) closeConfig();
  });

  document.getElementById("aliyun-config-form")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const accessKeyId = document.getElementById("input-accessKeyId").value;
    const accessKeySecret = document.getElementById("input-accessKeySecret").value;
    const regionId = document.getElementById("input-regionId").value;

    addConsoleLog("sys", "正在提交 OpenAPI 凭证...");

    try {
      const response = await fetch(`${API_BASE_URL}/api/config`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ accessKeyId, accessKeySecret, regionId })
      });
      const result = await response.json();

      if (!result.success) {
        addConsoleLog("err", `配置失败：${result.message}`);
        alert(`配置失败: ${result.message}`);
        return;
      }

      addConsoleLog("tx", result.message || "OpenAPI 配置已更新。");
      document.getElementById("input-accessKeySecret").value = "";
      closeConfig();
      await fetchSystemStatus();
      await fetchDevices(true);
    } catch (error) {
      addConsoleLog("err", "配置接口无响应。");
      console.error(error);
    }
  });

  document.getElementById("btn-reset-to-mock")?.addEventListener("click", async () => {
    if (!confirm("确认清除当前 OpenAPI 凭证吗？")) return;

    try {
      await fetch(`${API_BASE_URL}/api/config`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          accessKeyId: "",
          accessKeySecret: "",
          regionId: "cn-shanghai"
        })
      });
      addConsoleLog("sys", "凭证已清除，系统将回到本地仿真模式。");
      closeConfig();
      await fetchSystemStatus();
      await fetchDevices(true);
    } catch (error) {
      console.error(error);
    }
  });

  const devicesModal = document.getElementById("devices-modal");
  const closeDevices = () => closeDeviceForm();

  document.querySelector('.nav-item[data-target="dashboard"]')?.addEventListener("click", (event) => {
    event.preventDefault();
    showPage("dashboard");
  });

  document.getElementById("btn-open-devices-sidebar")?.addEventListener("click", (event) => {
    event.preventDefault();
    showPage("devices");
  });
  document.getElementById("btn-add-device-record")?.addEventListener("click", () => openDeviceForm());
  document.getElementById("btn-cancel-device-edit")?.addEventListener("click", closeDeviceForm);
  document.getElementById("btn-close-devices-drawer")?.addEventListener("click", closeDevices);

  devicesModal?.addEventListener("click", (event) => {
    if (event.target === devicesModal) closeDevices();
  });

  document.getElementById("add-device-form")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const id = document.getElementById("input-device-id").value.trim();
    const name = document.getElementById("input-device-name").value.trim();
    const type = document.getElementById("input-device-type").value;
    const productKey = document.getElementById("input-device-pk").value.trim();
    const deviceName = document.getElementById("input-device-dn").value.trim();

    const success = editingDeviceId
      ? await updateDevice(editingDeviceId, { name, type, productKey, deviceName })
      : await addDevice(id, name, type, productKey, deviceName);
    if (!success) return;

    event.target.reset();
    closeDeviceForm();
  });

  document.getElementById("device-select")?.addEventListener("change", (event) => {
    selectDevice(event.target.value);
  });
}
