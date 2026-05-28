const { app, BrowserWindow, dialog } = require("electron");
const path = require("path");
const { spawn } = require("child_process");
const http = require("http");

const SERVER_PORT = 5000;
const SERVER_URL = `http://127.0.0.1:${SERVER_PORT}`;

let mainWindow = null;
let serverProcess = null;
let embeddedServer = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1560,
    height: 980,
    minWidth: 1280,
    minHeight: 800,
    autoHideMenuBar: true,
    backgroundColor: "#0b1219",
    title: "Aliyun IoT Dashboard",
    webPreferences: {
      contextIsolation: true,
      sandbox: false
    }
  });

  mainWindow.on("closed", () => {
    mainWindow = null;
  });
}

function resolveServerRoot() {
  if (app.isPackaged) {
    return path.join(process.resourcesPath, "server");
  }
  return path.join(__dirname, "..", "server");
}

function resolveServerEntry() {
  return path.join(resolveServerRoot(), "server.js");
}

function waitForServer(url, timeoutMs = 30000) {
  const startedAt = Date.now();

  return new Promise((resolve, reject) => {
    const attempt = () => {
      const request = http.get(url, (response) => {
        response.resume();
        if (response.statusCode && response.statusCode < 500) {
          resolve();
          return;
        }
        response.destroy();
        retry();
      });

      request.on("error", retry);

      function retry() {
        if (Date.now() - startedAt >= timeoutMs) {
          reject(new Error("本地服务启动超时，无法连接到桌面仪表盘。"));
          return;
        }
        setTimeout(attempt, 500);
      }
    };

    attempt();
  });
}

async function startServerAndLoad() {
  if (app.isPackaged) {
    const serverModule = require(resolveServerEntry());
    embeddedServer = serverModule.startServer(SERVER_PORT);
    await waitForServer(SERVER_URL);
    await mainWindow.loadURL(SERVER_URL);
    return;
  }

  const serverRoot = resolveServerRoot();
  const nodeExecutable = process.execPath;
  serverProcess = spawn(nodeExecutable, ["server.js"], {
    cwd: serverRoot,
    stdio: "pipe",
    windowsHide: true,
    env: {
      ...process.env,
      ELECTRON_DESKTOP_RUNTIME: "1"
    }
  });

  let startupError = "";

  serverProcess.stderr.on("data", (chunk) => {
    startupError += chunk.toString();
  });

  serverProcess.on("exit", (code) => {
    if (code !== 0 && mainWindow) {
      dialog.showErrorBox(
        "Aliyun IoT Dashboard",
        `本地服务启动失败。\n\n退出码: ${code}\n\n${startupError || "未捕获到更多错误信息。"}`
      );
      app.quit();
    }
  });

  await waitForServer(SERVER_URL);
  await mainWindow.loadURL(SERVER_URL);
}

app.whenReady().then(async () => {
  createWindow();

  try {
    await startServerAndLoad();
  } catch (error) {
    dialog.showErrorBox("Aliyun IoT Dashboard", error.message);
    app.quit();
  }

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
      void mainWindow.loadURL(SERVER_URL);
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});

app.on("before-quit", () => {
  if (serverProcess && !serverProcess.killed) {
    serverProcess.kill();
  }
  if (embeddedServer) {
    embeddedServer.close();
    embeddedServer = null;
  }
});
