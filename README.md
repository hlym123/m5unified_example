# M5Unified 示例

这是一个使用 PlatformIO 的 `M5Unified` 示例项目，当前默认入口是 RTC 演示。

## 功能

- 初始化 `M5Unified`
- 读取并显示板载 RTC 时间
- 显示 RTC 掉电低压状态
- 通过 `BtnA` 用编译时间回写 RTC
- 通过 `BtnB` 立即刷新显示

## 硬件要求

- M5Stack 设备（建议带内置 RTC 的机型，例如 StopWatch / PaperColor）

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

### HAT 18650C 测试

StickC Plus 上的 HAT 18650C 测试使用独立环境（SDA=0，SCL=26），不会改变默认示例：

```bash
pio run -e m5stack-stickcplus-hat18650c
pio run -e m5stack-stickcplus-hat18650c -t upload
pio device monitor -b 115200
```

测试界面每秒刷新电池电压和电流。`BtnA` 切换充电开关，`BtnB` 在 500/1000/1500/2500 mA 之间循环。

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
├── example/
│   └── rtc_example.cpp  # RTC 示例源码
├── src/
│   └── main.cpp         # 当前默认编译入口（RTC demo）
└── README.md         # 本文件
```

## 依赖

- M5GFX 库（位于 `../M5GFX`）
- M5Unified 库（位于 `../M5Unified`）

## 说明

默认示例会在启动后尝试读取板载 RTC，并把当前编译时间同步到 RTC。
如果目标板没有可用 RTC，屏幕会显示 `RTC not found`。

