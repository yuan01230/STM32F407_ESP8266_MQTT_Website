# UI 模块拆分说明

本次将原先集中在 `main.c` / `ui_app.c` 的页面逻辑拆分为“底层复用 + 上层页面”结构，便于后续扩展与复用。

## 目录结构

- `Library/ui/ui_app.c/.h`
  - UI入口与按键路由（调度层）
- `Library/ui/ui_common.c/.h`
  - 底层公共能力（状态、计时、文件列表加载、编码转换）
- `Library/ui/ui_main.c/.h`
  - 主页面显示与运行时间刷新
- `Library/ui/ui_list.c/.h`
  - 文件列表页显示、上下选择、局部刷新
- `Library/ui/ui_view.c/.h`
  - 文件查看页显示、打开选中文件
- `Library/ui/ui_font_import.c/.h`
  - 字库导入流程、导入进度显示
- `Library/ui/ui_create_file.c/.h`
  - KEY2创建文件与结果预览页

## 分层关系

1. `main.c`
- 只保留硬件初始化、SD挂载、主循环调用
- 调用接口：
  - `UI_Init`
  - `UI_SetSdMounted`
  - `UI_FontSystemInit`
  - `UI_ShowMainPage`
  - `UI_Tick`
  - `UI_HandleKey`

2. `ui_app`（调度层）
- 按 `KeyName_t` 分发行为到各页面模块
- 不持有复杂渲染细节

3. `ui_common`（底层复用）
- 全局UI状态
- 文件系统根目录列表读取
- UTF-8 判定与 UTF-8 -> GB2312 转换

4. 页面实现层
- 每个页面模块只关注自己的显示与行为

## 原函数迁移对照

- `LCD_Show_Font_Test_Case` -> `ui_main.c`
- `LCD_Update_Test_Runtime` -> `ui_main.c`
- `LoadRootFileList` / `CountRootFiles` -> `ui_common.c`
- `LCD_Show_FileList_Page` -> `ui_list.c`
- `OpenSelectedFileAndShow` / `LCD_Show_FileView_Page` -> `ui_view.c`
- `Font_System_Init` / `LCD_Show_Font_Import_Progress` -> `ui_font_import.c`
- `CreateTestFileOnSDAndShow` / `LCD_Show_CreateFilePreview` -> `ui_create_file.c`
- 主循环按键 `switch` -> `ui_app.c` 的 `UI_HandleKey`

## 后续扩展建议

- 新页面可按相同模式新增 `ui_xxx.c/.h`
- 需要硬件无关复用时，优先放在 `ui_common`
- 需要跨页面共享状态时，优先通过 `ui_common.h` 管理
