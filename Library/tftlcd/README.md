# TFTLCD 驱动说明

## 1. 模块定位
当前目录下的 LCD 驱动，现阶段先整理两部分内容：
1. LCD 底层访问层；
2. LCD 基础绘图层。

本阶段明确不处理以下内容：
- 中文字库
- 中英混排
- UI 页面
- 触摸
- 图片显示
- SD 卡显示链路

这样拆分的目的是先把最底层、最基础、最容易复用的部分整理稳定，后续再在此基础上继续整理文字显示和更上层界面功能。

## 2. 文件结构
- `tftlcd_port.h` / `tftlcd_port.c`
  - 板级端口层。
  - 负责 LCD 复位、背光控制、FSMC 地址映射。
  - 如果更换开发板或修改 LCD 接线，优先调整这一层。

- `tftlcd_hx8357dn.h` / `tftlcd_hx8357dn.c`
  - LCD 控制器层。
  - 负责 HX8357DN 的写命令、写数据、写颜色、读 ID、初始化序列、方向设置和窗口设置。
  - 当前工程只整理 HX8357DN，不扩展多控制器框架。

- `tftlcd.h` / `tftlcd.c`
  - 公共驱动层。
  - 对外提供稳定接口。
  - 向上承接基础绘图、字符显示、中文显示、图片显示等功能调用。

- `tftlcd_example.c`
  - LCD 驱动使用示例。
  - 包含底层初始化示例、窗口写入示例和基础绘图示例。

## 3. LCD 调用关系
### 3.1 总体调用链
当前 LCD 驱动从底层到上层的调用关系可以理解为：

`main.c / 业务层`
→ `tftlcd.c` 中的公共接口层
→ `tftlcd_hx8357dn.c` 中的控制器层
→ `tftlcd_port.c` 中的板级端口层
→ FSMC 总线 / LCD 控制器寄存器 / LCD GRAM

也可以按职责拆开理解：
- 业务层
  - 只关心“我要清屏、画线、显示字符、显示图片”。
- 公共驱动层 `tftlcd.c`
  - 负责把业务需求转换成统一的 LCD 操作接口。
  - 例如清屏、画点、画线、画矩形、字符显示等都在这一层组织调用。
- 控制器层 `tftlcd_hx8357dn.c`
  - 负责把统一接口转换成 HX8357DN 的寄存器命令和数据写入流程。
- 端口层 `tftlcd_port.c`
  - 负责与板级硬件绑定，例如复位脚、背光脚、FSMC 地址映射。

### 3.2 初始化调用关系
典型初始化调用链如下：

`main.c`
→ `TFTLCD_Init()`
→ `TFTLCD_Port_Init()`
→ `HX8357DN_Init(&tftlcd_data)`
→ `LCD_Display_Dir()`
→ `HX8357DN_SetDisplayDir()`
→ `LCD_Clear(WHITE)`
→ `LCD_Set_Window()`
→ `HX8357DN_SetWindow()`
→ `LCD_WriteData_Color()`
→ `HX8357DN_WriteColor()`

说明：
- `TFTLCD_Port_Init()` 负责硬件复位、背光开启等板级动作。
- `HX8357DN_Init()` 负责 LCD 控制器寄存器初始化。
- `LCD_Display_Dir()` 和 `LCD_Clear()` 属于公共层对上提供的稳定接口。

### 3.3 基础绘图调用关系
以几个典型函数为例：

1. 清屏调用链：
`LCD_Clear()`
→ `LCD_Set_Window()`
→ `HX8357DN_SetWindow()`
→ `LCD_WriteSolidColor()`
→ `LCD_WriteData_Color()`
→ `HX8357DN_WriteColor()`

2. 画点调用链：
`LCD_DrawPoint()`
→ `LCD_Set_Window()`
→ `HX8357DN_SetWindow()`
→ `LCD_WriteData_Color()`
→ `HX8357DN_WriteColor()`

3. 画线调用链：
`LCD_DrawLine()`
→ 多次调用 `LCD_DrawPoint()`
→ `LCD_Set_Window()`
→ `HX8357DN_SetWindow()`
→ `LCD_WriteData_Color()`
→ `HX8357DN_WriteColor()`

4. 区域颜色填充调用链：
`LCD_Color_Fill()`
→ `LCD_Set_Window()`
→ `HX8357DN_SetWindow()`
→ 循环调用 `LCD_WriteData_Color()`
→ `HX8357DN_WriteColor()`

### 3.4 上层显示功能调用关系
虽然当前阶段还没有继续重构文字显示和图片显示，但从现有代码结构上看，它们的调用关系也是同一条链：

`LCD_ShowChar()` / `LCD_ShowString()` / `LCD_ShowChinese()` / `LCD_ShowPicture()`
→ 调用基础绘图函数，如 `LCD_DrawPoint()`、`LCD_DrawFRONT_COLOR()`、`LCD_Fill()`、`LCD_Color_Fill()`
→ 再进入 `LCD_Set_Window()` / `LCD_WriteData_Color()`
→ 再进入 `HX8357DN_SetWindow()` / `HX8357DN_WriteColor()`
→ 最终写入 LCD 显存

这说明后续继续整理字符层、中文层、图片层时，只要基础绘图层稳定，上层就可以在不关心硬件细节的情况下复用底层能力。

### 3.5 分层职责表
| 层级 | 代表文件 | 上层输入 | 对下输出 | 依赖内容 | 本层不应做什么 |
| --- | --- | --- | --- | --- | --- |
| 业务层 | `main.c`、应用功能模块 | “显示什么内容” | 调用 `TFTLCD_Init()`、`LCD_Clear()`、`LCD_DrawLine()`、`LCD_ShowString()` 等公共接口 | 驱动头文件 `tftlcd.h` | 不直接操作 LCD 寄存器，不关心 FSMC 地址和控制器寄存器细节 |
| 公共驱动层 | `tftlcd.c` / `tftlcd.h` | 清屏、画点、画线、显示字符、显示图片等通用需求 | 调用 `HX8357DN_*()` 控制器接口 | 控制器层、运行时参数 `tftlcd_data` | 不直接绑定具体引脚，不把业务逻辑写进驱动 |
| 控制器层 | `tftlcd_hx8357dn.c` / `.h` | 公共层传下来的统一 LCD 操作 | 写命令、写数据、设置窗口、设置方向、初始化寄存器 | HX8357DN 指令集、端口层提供的总线访问 | 不处理业务显示流程，不关心按键、菜单、页面逻辑 |
| 板级端口层 | `tftlcd_port.c` / `.h` | 控制器层要求的复位、背光、地址访问能力 | GPIO 电平控制、FSMC 地址映射 | `main.h` 中的引脚定义、FSMC 硬件连接 | 不写控制器初始化时序，不组织绘图流程 |
| 硬件层 | FSMC 总线、LCD 控制器、LCD GRAM | 命令、数据、时序、电平 | 屏幕实际显示结果 | 原理图、芯片手册、时序参数 | 不承载任何软件抽象职责 |

从这个表可以看出，越往上越关注“功能”，越往下越关注“硬件访问”。后续你继续重构字符层、中文层、图片层时，也应保持这个边界，不要把页面逻辑重新写回底层驱动里。

## 4. 当前重构边界
### 4.1 已整理内容
- LCD 端口层与控制器层分离
- LCD 初始化流程整理
- 屏幕方向设置
- 写窗口设置
- 像素读写
- 全屏清屏
- 区域清除
- 单色区域填充
- 颜色数组填充
- 画点
- 画线
- 画矩形
- 画圆
- 十字标记
- ASCII 单字符显示
- 十进制数字显示
- ASCII 字符串显示

### 4.2 暂不整理内容
以下函数仍保留在当前公共层文件中，但不作为本阶段整理重点：
- `LCD_ShowTextMixed()`
- `LCD_DrawOneChinese()`
- `LCD_ShowChinese()`
- `LCD_ShowFontHZ()`
- `LCD_ShowPicture()`

## 5. 初始化依赖
调用 `TFTLCD_Init()` 前，需要保证以下初始化已经完成：
1. `HAL_Init()`
2. 系统时钟初始化
3. `MX_GPIO_Init()`
4. `MX_FSMC_Init()`

如果上述初始化不完整，LCD 可能表现为：
- 背光不亮
- 屏幕无显示
- 花屏
- 无法正常读写显存

## 6. 对外接口说明
### 6.1 底层控制接口
- `TFTLCD_Init()`
  - 完成 LCD 复位、控制器初始化、显示方向设置和默认清屏。

- `LCD_Display_Dir(u8 dir)`
  - 设置显示方向。
  - `0` 表示竖屏，`1` 表示横屏。

- `LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height)`
  - 设置 LCD 写窗口。
  - 注意：历史参数名 `width/height` 实际表示结束坐标 `ex/ey`，当前为了兼容旧代码，不修改接口名称。

- `LCD_WriteCmd()` / `LCD_WriteData()` / `LCD_WriteCmdData()` / `LCD_WriteData_Color()`
  - 提供底层命令和数据写入能力。
  - 一般用于驱动内部或调试，不建议业务层频繁直接调用。

### 6.2 基础绘图接口
- `LCD_Clear(u16 color)`
  - 整屏填充指定颜色。
  - 常用于开机清屏、页面整体切换。

- `LCD_ClearArea(x0, y0, x1, y1)`
  - 用白色清除指定矩形区域。
  - 常用于局部刷新前先擦除旧显示内容。

- `LCD_Fill(x0, y0, x1, y1, color)`
  - 使用单一颜色填充矩形区域。
  - 适合绘制背景块、底栏、状态框。

- `LCD_Color_Fill(x0, y0, x1, y1, color_buf)`
  - 使用颜色数组填充矩形区域。
  - 适合显示图标缓存或软件生成的小尺寸图像。

- `LCD_DrawPoint(x, y)`
  - 使用当前前景色 `FRONT_COLOR` 绘制一个像素点。

- `LCD_DrawFRONT_COLOR(x, y, color)`
  - 直接使用指定颜色绘制一个像素点。

- `LCD_ReadPoint(x, y)`
  - 读取指定像素点颜色。

- `LCD_DrawLine(x1, y1, x2, y2)`
  - 使用当前前景色绘制直线。

- `LCD_DrawLine_Color(x1, y1, x2, y2, color)`
  - 使用指定颜色绘制直线。

- `LCD_DrawRectangle(x1, y1, x2, y2)`
  - 使用当前前景色绘制空心矩形。

- `LCD_Draw_Circle(x0, y0, r)`
  - 使用当前前景色绘制空心圆。

- `LCD_DrowSign(x, y, color)`
  - 在指定位置绘制十字标记。
  - 适合做触摸调试点或关键坐标标记。

### 6.3 ASCII 字符显示接口
- `LCD_ShowChar(x, y, ch, size, mode)`
  - 显示单个 ASCII 字符。
  - 支持 `12`、`16`、`24` 三种字体高度。
  - `mode=0` 为非叠加显示，`mode=1` 为叠加显示。

- `LCD_ShowNum(x, y, num, len, size)`
  - 显示十进制无符号整数。
  - 高位前导 0 不显示，而是用空格占位。

- `LCD_ShowxNum(x, y, num, len, size, mode)`
  - 显示十进制无符号整数。
  - 可控制高位前导 0 是显示为空格还是字符 `0`。
  - `mode bit7` 控制前导位填充方式，`mode bit0` 控制叠加显示方式。

- `LCD_ShowString(x, y, width, height, size, str)`
  - 在指定矩形区域内显示 ASCII 字符串。
  - 超出单行宽度时自动换行。
  - 超出显示区域高度后停止输出。

## 7. 本次整理的实现要点
### 7.1 分层目标
通过“端口层 + 控制器层 + 公共层”的分层方式，实现以下目标：
- 板级硬件细节与绘图接口解耦
- 控制器寄存器操作与业务代码解耦
- 对上层保留稳定 API，降低已有工程修改成本

### 7.2 基础绘图层整理点
这次对基础绘图层主要做了以下整理：
- 增加统一的坐标有效性判断
- 增加统一的矩形区域排序与裁剪处理
- 抽取连续写单色像素的公共辅助函数
- 优化 `LCD_Color_Fill()`，避免每写一个像素就重新设置一次窗口
- 为基础绘图函数补充统一风格的中文注释和功能说明

### 7.3 ASCII 字符层整理点
这次对 ASCII 字符层主要做了以下整理：
- 抽取统一的字符宽度获取辅助函数
- 抽取统一的 ASCII 字模选择辅助函数
- 明确只支持 `12`、`16`、`24` 三种字体高度
- 为字符、数字、字符串显示函数补充统一风格的中文注释
- 保持旧接口不变，降低上层迁移成本

## 8. 使用示例
### 8.1 初始化验证示例
参考 `tftlcd_example.c` 中的 `TFTLCD_Example_BasicInit()`：
- 初始化 LCD
- 全屏清成白色
- 在四个角各画一个红点
- 用于验证初始化、方向和画点功能是否正常

### 8.2 窗口写入验证示例
参考 `tftlcd_example.c` 中的 `TFTLCD_Example_WindowFill()`：
- 设置一个 50x50 的显示窗口
- 向窗口连续写入蓝色像素
- 用于验证窗口设置和连续写显存流程

### 8.3 基础绘图验证示例
参考 `tftlcd_example.c` 中的 `TFTLCD_Example_BasicDraw()`：
- 测试单色区域填充
- 测试画点
- 测试直线
- 测试矩形
- 测试圆
- 测试十字标记

### 8.4 `main.c` 字符显示验证
当前 `main.c` 已加入 LCD 按键测试：
- `KEY0`：清屏和区域填充测试
- `KEY1`：画点和画线测试
- `KEY2`：矩形和圆测试
- `KEY_UP`：字符串与数字显示测试

其中 `KEY_UP` 可直接验证以下接口：
- `LCD_ShowString()`
- `LCD_ShowNum()`
- `LCD_ShowxNum()`

## 9. 建议的后续整理顺序
建议下一步继续在当前底层和基础绘图层之上，按以下顺序整理：
1. 中文显示
2. 中英混排
3. 图片显示入口
4. UI 层依赖关系梳理

等这些显示接口整理稳定后，再继续处理图片显示、UI 层和触摸相关功能，整体风险会更可控。
