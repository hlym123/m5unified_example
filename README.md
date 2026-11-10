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

### LSM6DS3TR-C IMU

`m5stack-corep4x-imu` 环境显示六轴数据，每秒输出温度及本周期读取的新加速度/陀螺仪样本数。
需要使用包含 `m5::imu_lsm6ds3trc` 支持的 M5Unified；启动日志会输出实际检测的类型和 I2C 地址。

```bash
pio run -e m5stack-corep4x-imu
pio run -e m5stack-corep4x-imu -t upload --upload-port <port>
pio device monitor --port <port> --baud 115200
```

LSM6DS3TR-C 的 WHO_AM_I 应为 `0x6A`，I2C 地址为 `0x6A` 或 `0x6B`。
默认配置为 104 Hz、加速度 ±8 g、陀螺仪 ±2000 dps；配置寄存器日志应为
`ctrl1_xl=0x4C ctrl2_g=0x4C ctrl3_c=0x44`。温度按 `25 + raw / 256` 换算。
传感器通常每秒产生约 104 组新六轴样本，日志中的 `new` 计数可用于排除数据未刷新的情况。

CoreP4X 上的加速度和陀螺仪使用相同轴映射：相对芯片原始轴，
输出为 `X=-Y、Y=-X、Z=-Z`。初始化会等待陀螺仪启动稳定并丢弃最初的样本；
启动时请保持设备静止，避免影响 M5Unified 的初始零偏校准。

轴向验收时，依次将屏幕朝上、朝下以及四条边朝上，静置检查对应轴约为 ±1 g，
其他两轴接近 0；绕单轴转动检查陀螺仪对应轴及正负方向。

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
