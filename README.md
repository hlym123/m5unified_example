# M5GFX 简单示例

这是一个使用 PlatformIO 的 M5GFX 简单使用示例项目。

## 功能

- 初始化 M5GFX 显示屏
- 显示文本
- 绘制基本图形：
  - 矩形（空心和填充）
  - 圆形（空心和填充）
  - 三角形（线条和填充）
- 动态绘制随机颜色的点

## 硬件要求

- M5Stack 设备（支持 M5GFX）
- 或者使用原生平台（需要 SDL2）进行模拟

## 编译和上传

### 使用 PlatformIO

1. 安装 PlatformIO（如果还没有安装）

2. 编译项目：
```bash
pio run
```

3. 上传到设备：
```bash
pio run -t upload
```

4. 查看串口输出：
```bash
pio device monitor
```

### 环境选择

- `esp32s3_arduino` - ESP32-S3 开发板（默认）
- `esp32s3_StickS3` - M5Stack StickS3
- `esp32_arduino` - ESP32 开发板
- `esp32c3_arduino` - ESP32-C3 开发板
- `native` - 本地模拟（需要 SDL2）

选择不同的环境：
```bash
pio run -e esp32s3_StickS3
```

## 项目结构

```
m5unified_example/
├── platformio.ini    # PlatformIO 配置文件
├── src/
│   └── main.cpp      # 主程序代码
└── README.md         # 本文件
```

## 依赖

- M5GFX 库（位于 `../M5GFX`）
- M5Unified 库（可选，位于 `../M5Unified`）

## 说明

此示例展示了 M5GFX 的基本绘图功能，适合初学者学习如何使用 M5GFX 库进行图形绘制。

