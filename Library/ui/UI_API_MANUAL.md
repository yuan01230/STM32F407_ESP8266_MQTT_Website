> 文档导航
> - [UI 目录说明](./README.md)
> - [UI 函数调用关系](./UI_FUNCTION_CALLS.md)
> - [UI API 手册](./UI_API_MANUAL.md)
# UI API 手册

## 1. 说明

本文档整理 `Library/ui` 目录下的对外 API（`.h` 中声明的函数）。

- 适用工程：`13_Font`
- UI 层入口：`ui_app.h`
- 编码说明：中文字符串建议使用 UTF-8 源码字符串，并通过 `UI_LCD_ShowUtf8` 显示到 LCD

---

## 2. 模块与调用关系（简版）

```c
UI_Init();
UI_SetSdMounted(sd_ok);
UI_FontSystemInit();
UI_ShowMainPage();
UI_PrintHelp();

while (1) {
    UI_Tick();
    UI_HandleKey(KEY_Scan(0));
}
```

---

## 3. `ui_app.h`（UI 总入口）

### 3.1 `void UI_Init(void)`
初始化 UI 页面状态与计时基线。

示例：
```c
UI_Init();
```

### 3.2 `void UI_SetSdMounted(uint8_t mounted)`
同步 SD 挂载状态到 UI 层（`1=已挂载，0=未挂载`）。

示例：
```c
FRESULT fr = f_mount(&SDFatFS, SDPath, 1);
UI_SetSdMounted(fr == FR_OK ? 1U : 0U);
```

### 3.3 `void UI_FontSystemInit(void)`
启动阶段字体系统初始化：检查外部 Flash 字库，必要时从 SD 导入。

示例：
```c
UI_SetSdMounted(1U);
UI_FontSystemInit();
```

### 3.4 `void UI_ShowMainPage(void)`
显示主页面，并刷新主页面运行时显示区。

示例：
```c
UI_ShowMainPage();
```

### 3.5 `void UI_PrintHelp(void)`
通过串口打印按键帮助。

示例：
```c
UI_PrintHelp();
```

### 3.6 `void UI_Tick(void)`
UI 周期任务：处理倒计时、自动回主页面、秒级局部刷新。

示例：
```c
while (1) {
    UI_Tick();
}
```

### 3.7 `void UI_HandleKey(KeyName_t key)`
UI 按键分发入口，根据当前页面和按键执行动作。

示例：
```c
KeyName_t key = KEY_Scan(0);
UI_HandleKey(key);
```

---

## 4. `ui_main.h`（主页面）

### 4.1 `void UI_MainPage_Show(void)`
绘制主页面主体内容（标题、示例文本、按键提示、状态区）。

示例：
```c
UI_MainPage_Show();
```

### 4.2 `void UI_MainPage_UpdateRuntime(uint32_t seconds)`
仅刷新主页面“运行时间”区域（局部刷新）。

示例：
```c
uint32_t sec = HAL_GetTick() / 1000U;
UI_MainPage_UpdateRuntime(sec);
```

---

## 5. `ui_list.h`（文件列表页）

### 5.1 `void UI_ListPage_Show(void)`
显示/刷新文件列表页（含防闪烁局部刷新策略）。

示例：
```c
(void)UI_LoadRootFileList();
UI_EnterPage(UI_PAGE_FILE_LIST);
UI_ListPage_Show();
```

### 5.2 `void UI_List_SelectDown(void)`
列表选择向下移动（循环）。

示例：
```c
UI_List_SelectDown();
UI_ListPage_Show();
```

### 5.3 `void UI_List_SelectUp(void)`
列表选择向上移动（循环）。

示例：
```c
UI_List_SelectUp();
UI_ListPage_Show();
```

---

## 6. `ui_view.h`（文件查看页）

### 6.1 `void UI_View_OpenSelected(void)`
打开当前选中文件并切换到查看页。

示例：
```c
if (g_file_count > 0U) {
    UI_View_OpenSelected();
}
```

### 6.2 `void UI_View_Show(uint16_t index)`
显示指定索引文件内容（自动区分文本/UTF-8/hex 摘要分支）。

示例：
```c
UI_View_Show(0U);
```

---

## 7. `ui_font_import.h`（字库导入）

### 7.1 `void UI_Font_ImportSystemInit(uint8_t sd_ready)`
系统启动时字库初始化逻辑（字库不存在时尝试导入）。

示例：
```c
UI_Font_ImportSystemInit(g_sd_mounted);
```

### 7.2 `void UI_Font_RunImportFromMain(void)`
主页面触发手动重新导入字库（通常绑定 KEY_UP）。

示例：
```c
UI_Font_RunImportFromMain();
UI_ShowMainPage();
```

---

## 8. `ui_create_file.h`（创建测试文件）

### 8.1 `void UI_CreateTestFileAndShow(void)`
创建 `测试专用.txt` 并写入 RTC + 中英文内容，然后显示结果页。

示例：
```c
UI_CreateTestFileAndShow();
UI_ShowMainPage();
```

---

## 9. `ui_common.h`（公共工具与状态）

### 9.1 `void UI_EnterPage(UiPage_t page)`
切换页面并刷新无操作计时。

示例：
```c
UI_EnterPage(UI_PAGE_FILE_LIST);
```

### 9.2 `uint32_t UI_GetCurrentSeconds(void)`
获取系统运行秒数（`HAL_GetTick()/1000`）。

示例：
```c
uint32_t now_sec = UI_GetCurrentSeconds();
```

### 9.3 `uint32_t UI_GetRemainSeconds(void)`
获取自动返回剩余秒数。

示例：
```c
uint32_t remain = UI_GetRemainSeconds();
```

### 9.4 `uint16_t UI_CountRootFiles(void)`
统计 SD 根目录普通文件数。

示例：
```c
uint16_t total = UI_CountRootFiles();
```

### 9.5 `uint16_t UI_LoadRootFileList(void)`
加载 SD 根目录文件到缓存（`g_file_entries[]`）。

示例：
```c
uint16_t count = UI_LoadRootFileList();
if (count > 0U) {
    g_selected_index = 0U;
}
```

### 9.6 `uint8_t UI_IsValidUtf8(const uint8_t *buf, uint16_t len)`
判断缓冲区是否为有效 UTF-8。

示例：
```c
uint8_t ok = UI_IsValidUtf8(data, data_len);
```

### 9.7 `uint8_t UI_Utf8DecodeCodepoint(...)`
解码单个 UTF-8 码点。

示例：
```c
uint16_t consumed = 0;
uint32_t cp = 0;
if (UI_Utf8DecodeCodepoint(p, left, &consumed, &cp)) {
    // cp 可用
}
```

### 9.8 `uint8_t UI_Utf8ToGb2312(...)`
将 UTF-8 字节流转为 GB2312 缓冲区。

示例：
```c
uint8_t out[256];
if (UI_Utf8ToGb2312(in, in_len, out, sizeof(out))) {
    LCD_ShowTextMixed(8, 100, 304, 16, out);
}
```

### 9.9 `uint8_t UI_TextUtf8ToGb2312(...)`
将 UTF-8 C 字符串转为 GB2312（以 `\0` 结尾）。

示例：
```c
uint8_t gb[128];
UI_TextUtf8ToGb2312("中文测试", gb, sizeof(gb));
```

### 9.10 `void UI_LCD_ShowUtf8(...)`
直接显示 UTF-8 文本到 LCD（内部自动转 GB2312）。

示例：
```c
UI_LCD_ShowUtf8(8, 120, 304, 16, "状态: 文件创建成功");
```

---

## 10. 建议实践

- 在 `main.c` 里只保留流程组织：初始化、循环调度、按键采集。
- UI 页面逻辑优先走 `ui_app` 路由，不要在 `main.c` 直接拼页面细节。
- 涉及中文显示时，优先用 `UI_LCD_ShowUtf8`，避免编码不一致。

