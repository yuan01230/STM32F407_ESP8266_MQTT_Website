# STM32F407 SD 卡文件浏览与图片预览项目

## 项目简介

本项目基于 `STM32F407 + TFT LCD + SDIO + FATFS`，实现了一套可运行的嵌入式文件浏览与图片预览示例。当前工程已经具备：

- `SD` 卡挂载与目录浏览
- 普通文本文件查看
- 字库从 `SD` 卡导入到外部 `W25Q128`
- `BMP` 图片运行时预览
- `JPG/JPEG` 图片运行时预览
- 大图等比缩小、小图原尺寸居中显示

## 功能总览

| 功能类别 | 当前实现 | 说明 |
| --- | --- | --- |
| 文件系统 | `FATFS + SDIO` | 从 `0:/` 根目录开始浏览 |
| 页面体系 | `Main / List / View` | 主页面、文件列表页、文件查看页 |
| 目录浏览 | 已支持 | 可进入任意子目录，支持 `[..]` 返回上一级 |
| 文本查看 | 已支持 | 非图片文件按文本或十六进制摘要显示 |
| 图片格式 | `BMP`、`JPG`、`JPEG` | 其余格式当前不支持运行时预览 |
| 图片缩放 | 已支持 | 大图等比缩小，小图不放大，预览区居中 |
| 中文显示 | 已支持 | 基于 `GB2312 + HZK16` 字库 |
| 字库导入 | 已支持 | 从 `SD` 卡导入到外部 `W25Q128` |

## 页面与按键说明

| 页面 | `KEY0` | `KEY1` | `KEY2` | `KEY_UP` |
| --- | --- | --- | --- | --- |
| 主页面 `Main` | 进入文件浏览器 | 切换主页面演示文字颜色 | 创建测试文件 | 重新导入字库 |
| 列表页 `List` | 返回主页面 | 选中下一项 | 进入目录 / 打开文件 / 处理 `[..]` | 选中上一项 |
| 查看页 `View` | 返回列表页 | 无 | 返回列表页 | 无 |

## 图片预览规则

| 项目 | 规则 |
| --- | --- |
| 预览入口 | 在列表页选中图片文件后按 `KEY2` |
| 支持格式 | `.bmp`、`.jpg`、`.jpeg` |
| 大图显示 | 按原图比例缩小到预览区域内 |
| 小图显示 | 保持原尺寸，不放大 |
| 对齐方式 | 在预览区域中居中显示 |
| 背景处理 | 每次预览前先清空预览区域，避免残影 |

## JPG 使用约束

当前 `JPG/JPEG` 预览基于 `TJpgDec` 软件解码器，只保证常规 `Baseline JPEG` 稳定可用。

| 项目 | 建议 |
| --- | --- |
| 推荐编码 | `Baseline JPEG` |
| 推荐颜色模式 | `RGB / 常规照片导出` |
| 推荐尺寸 | `320x240`、`240x320`、`240x240` |
| 不推荐格式 | `Progressive JPEG`、`CMYK JPEG`、特殊采样方式 JPEG |

### 图片预览状态提示

| 状态文本 | 含义 |
| --- | --- |
| `Image Preview: OK` | 图片已成功打开并显示 |
| `Image Preview: JPG UNSUP` | JPEG 编码方式不在当前解码器支持范围内 |
| `Image Preview: JPG NOMEM` | JPEG 解码工作区内存不足 |
| `Image Preview: JPG FORMAT` | JPEG 文件结构损坏或头信息非法 |
| `Image Preview: OPEN FAIL` | 文件打开失败 |
| `Image Preview: READ ERR` | 文件读取失败 |
| `Image Preview: RANGE ERR` | 预览区域参数非法 |

## 核心模块说明

| 模块 | 路径 | 作用 |
| --- | --- | --- |
| `ui` | `Library/ui/` | 页面切换、按键分发、目录浏览、文件查看 |
| `Picture` | `Library/Picture/` | `BMP/JPG` 解码与预览显示 |
| `SD_RW` | `Library/SD_RW/` | `SD` 文件展示相关页面入口 |
| `tftlcd` | `Library/tftlcd/` | LCD 底层驱动与基础绘图接口 |
| `font_storage` | `Library/font_storage/` | 字库导入与外部 Flash 字模读取 |
| `font_codec` | `Library/font_codec/` | `GB2312 / ASCII` 编码解析 |

## 文档索引

| 文档 | 路径 | 说明 |
| --- | --- | --- |
| 项目总览与操作手册 | [doc/PROJECT_GUIDE.md](/F:/CLion/Clion_Code/14_Picture/doc/PROJECT_GUIDE.md) | 表格化说明页面、按键、模块和测试建议 |
| 图片模块说明 | [Library/Picture/README.md](/F:/CLion/Clion_Code/14_Picture/Library/Picture/README.md) | 图片预览能力、JPEG 约束、状态码说明 |
| SD 卡图片显示说明 | [Library/Picture/SD_CARD_LCD_IMAGE_DISPLAY.md](/F:/CLion/Clion_Code/14_Picture/Library/Picture/SD_CARD_LCD_IMAGE_DISPLAY.md) | 从 SD 卡读取图片到 LCD 的原理与工程说明 |

## 构建方式

```bash
cmake --preset Debug
cmake --build --preset Debug
```

## 硬件依赖

| 类别 | 说明 |
| --- | --- |
| MCU | `STM32F407` |
| 显示 | TFT LCD 并口屏 |
| 存储 | `SD` 卡 + 外部 `W25Q128` |
| 输入 | `KEY0 / KEY1 / KEY2 / KEY_UP` |
