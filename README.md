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

## CoreP4X 调试记录

### USB 已连接但未打开串口时触摸卡顿

- 现象：USB 已连接并枚举，但上位机未打开串口时，连续触摸后画点和界面更新会出现卡顿。打开串口监视器或拔掉 USB 后不复现。
- 原因：问题由 USB HWCDC 发送缓冲区反压引起，与触摸驱动无关。未打开串口时，高频日志会填满发送缓冲区，默认写入超时会阻塞主循环。
- 处理：CoreP4X 测试例程在初始化串口后设置非阻塞发送：

```cpp
Serial.begin(115200);
Serial.setTxTimeoutMs(0);
```

持续输出的调试日志统一使用 `Serial.printf()`。上位机不消费串口数据时可能丢弃日志，但不会再阻塞触摸和界面更新。

- 验证：画点测试在 USB 已连接但串口未打开、串口已打开以及 USB 未连接三种状态下均可持续响应触摸。

## 依赖

- M5GFX 库（位于 `../M5GFX`）
- M5Unified 库（位于 `../M5Unified`）

## 说明

默认示例会在启动后尝试读取板载 RTC，并把当前编译时间同步到 RTC。
如果目标板没有可用 RTC，屏幕会显示 `RTC not found`。
