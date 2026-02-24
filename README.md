# OpenClaw WiFi Monitor

ESP32-S2 通过 WiFi 连接并监控 OpenClaw Gateway 状态，实时显示在 SSD1306 OLED 显示屏上。

## 📋 项目概述

本项目是一个物联网监控设备，专门用于监控 OpenClaw AI Agent 网关的运行状态。ESP32-S2 通过 WiFi 连接到局域网，定期检测 OpenClaw Gateway 的可用性，并将结果显示在 128x64 像素的 OLED 显示屏上。

## ✨ 功能特性

### 核心功能
- **WiFi 连接管理**：自动连接配置的 WiFi 网络，支持断线自动重连
- **Gateway 状态监控**：每 5 秒通过 TCP 连接检测 OpenClaw Gateway 是否在线
- **信号强度显示**：实时显示 WiFi RSSI (dBm) 和可视化信号强度条
- **连接统计**：记录检测成功/失败次数
- **本地 IP 显示**：显示 ESP32 获取到的局域网 IP 地址

### 显示界面
```
  OpenClaw Mon
────────────────
WiFi: ||| -45 dBm
Gateway: [ONLINE]
IP: 192.168.2.xxx
OK:12 ERR:0
```

### 显示说明
| 项目 | 说明 |
|------|------|
| **WiFi** | 信号条 + RSSI 值 (dBm)<br>`||||` 优秀(>-50dBm) `|||` 良好(-60dBm)<br>`||` 一般(-70dBm) `|` 较弱(-80dBm) |
| **Gateway** | `[ONLINE]` 或 `[OFFLINE]` |
| **IP** | ESP32 的本地 IP 地址 |
| **OK/ERR** | 成功/失败检测次数统计 |

## 🔧 硬件要求

### 硬件清单
- ESP32-S2 开发板
- SSD1306 128x64 OLED 显示屏 (I2C接口)
- 杜邦线若干

### 接线图
| SSD1306 | ESP32-S2 | 说明 |
|---------|----------|------|
| VCC     | 3.3V     | 电源正极 |
| GND     | GND      | 电源负极 |
| SCL     | GPIO19   | I2C 时钟线 |
| SDA     | GPIO18   | I2C 数据线 |

## 💻 软件配置

### 开发环境
- **ESP-IDF**: v6.0
- **目标芯片**: ESP32-S2
- **开发语言**: C

### 配置参数

所有配置参数现已移至 ESP-IDF 的 menuconfig 系统，无需修改源代码。

#### 使用 menuconfig 配置参数

1. 打开配置菜单：
   ```bash
   idf.py menuconfig
   ```

2. 进入 "OpenClaw Monitor Configuration" 菜单，设置以下参数：
   - **WiFi SSID**: WiFi 网络名称
   - **WiFi Password**: WiFi 密码
   - **OpenClaw Gateway Host**: OpenClaw Gateway 的 IP 地址（默认 192.168.2.195）
   - **OpenClaw Gateway Port**: 端口号（默认 18789）
   - **Check Interval (ms)**: 检查间隔（默认 5000 毫秒）
   - **Display I2C Address**: OLED 显示屏的 I2C 地址（默认 0x3C）
   - **SCL GPIO Num**: I2C SCL 引脚号（默认 19）
   - **SDA GPIO Num**: I2C SDA 引脚号（默认 18）
   - **I2C Port Number**: I2C 端口号（0 或 1，默认 0）
   - **I2C Frequency (Hz)**: I2C 时钟频率（默认 400000 Hz）
   - **WiFi Max Retry Count**: WiFi 连接最大重试次数（默认 5）
   - **Static IP Address**: ESP32 静态 IP 地址（默认 192.168.1.200）
   - **Static Gateway Address**: 网关地址（默认 192.168.1.139）
   - **Static Netmask**: 子网掩码（默认 255.255.255.0）

3. 保存配置并退出，配置将自动保存到 `sdkconfig` 文件中。

4. 重新编译项目：
   ```bash
   idf.py build
   ```

## 🚀 快速开始

### 1. 克隆/进入项目目录
```bash
cd ~/esp-projects/openclaw-monitor
```

### 2. 设置目标芯片
```bash
idf.py set-target esp32s2
```

### 3. 初始化配置文件
首次使用或需要重置配置时，从默认模板创建配置文件：
```bash
cp sdkconfig.defaults sdkconfig
```

### 4. 配置参数（可选）
如需自定义 WiFi、Gateway 地址等参数，运行：
```bash
idf.py menuconfig
```
进入 "OpenClaw Monitor Configuration" 菜单修改相应参数。

### 5. 构建项目
```bash
idf.py build
```

### 6. 烧录固件
```bash
idf.py -p /dev/cu.usbserial-310 flash
```

### 7. 查看串口输出（调试用）
```bash
idf.py -p /dev/cu.usbserial-310 monitor
```
按 `Ctrl+]` 退出监控。

## 🔄 工作流程

```
启动
  ↓
初始化 I2C → 初始化 OLED → 显示 "Connecting..."
  ↓
初始化 WiFi → 连接 "Pi-AP"
  ↓
获取 IP 地址 → 显示主界面
  ↓
┌─────────────────────────────────────────┐
│  每 5 秒循环:                           │
│  1. 更新 WiFi 信号强度                  │
│  2. TCP 连接 Gateway:18789              │
│  3. 更新 OK/ERR 计数                    │
│  4. 刷新 OLED 显示                      │
└─────────────────────────────────────────┘
```

## 📊 监控原理

### Gateway 检测机制
ESP32 通过 TCP 连接到 OpenClaw Gateway 的端口来判断服务状态：

```c
// 创建 TCP socket
socket(AF_INET, SOCK_STREAM, 0)

// 设置超时时间 3 秒
tv.tv_sec = 3

// 尝试连接 Gateway
connect(sock, gateway_addr, port)

// 连接成功 = Gateway ONLINE
// 连接失败 = Gateway OFFLINE
```

### WiFi 信号强度
通过 ESP-IDF WiFi API 获取 RSSI 值：
```c
esp_wifi_sta_get_ap_info(&ap_info);
ap_info.rssi  // 信号强度 (dBm)
```

## 🛠️ 故障排查

| 问题现象 | 可能原因 | 解决方案 |
|---------|---------|---------|
| **WiFi: Disconnected** | 找不到 WiFi 网络 | 确认 "Pi-AP" 是 2.4GHz 频段（ESP32-S2 不支持 5GHz） |
| **Gateway: [OFFLINE]** | OpenClaw 未运行 | 在主机上运行 `openclaw gateway start` |
| 屏幕无显示 | I2C 地址错误 | 尝试修改地址为 `0x3D` |
| 屏幕显示偏移 | SSD1306 兼容性问题 | 代码已添加 X_OFFSET=2 修复偏移 |
| 串口输出乱码 | 波特率不匹配 | 确保使用 115200 波特率 |

### WiFi 错误码参考
| 错误码 | 含义 | 说明 |
|-------|------|------|
| 201 | NO_AP_FOUND | 找不到 WiFi 热点 |
| 202 | AUTH_FAIL | WiFi 密码错误 |
| 203 | ASSOC_FAIL | 关联失败 |
| 204 | HANDSHAKE_TIMEOUT | 握手超时 |

## 📝 自定义配置

OpenClaw Monitor 项目采用 ESP-IDF 风格的配置分层策略，支持多种配置方式：

### 配置分层说明

项目包含以下配置文件：

1. **`sdkconfig.defaults`** - 基础默认配置
   - 包含**非敏感**的硬件配置和示例值
   - 提交到 Git 仓库，作为项目基准配置
   - 包括：I2C 引脚、检查间隔、示例 IP 地址等

2. **`sdkconfig.ci`** - CI/CD 环境配置
   - 使用环境变量占位符，支持自动化部署
   - 提交到 Git 仓库，用于 CI/CD 流水线
   - 包括：WiFi 凭证、网络地址等敏感信息的占位符

3. **`sdkconfig`** - 本地开发配置
   - **不提交**到 Git 仓库（在 `.gitignore` 中排除）
   - 每个开发者/设备本地的实际配置
   - 通过 `idf.py menuconfig` 或手动编辑创建

### 新项目初始化

首次使用项目时，选择以下方式之一：

#### 方式一：使用 menuconfig（推荐）
```bash
# 从默认模板创建配置文件
cp sdkconfig.defaults sdkconfig

# 交互式配置所有参数
idf.py menuconfig

# 构建和烧录
idf.py build
idf.py flash
```

#### 方式二：手动配置
```bash
# 复制模板并手动编辑
cp sdkconfig.defaults sdkconfig
# 编辑 sdkconfig 文件，设置真实参数
# 或者直接使用示例值开始测试
idf.py build flash
```

#### 方式三：CI/CD 环境
```bash
# 在 CI/CD 环境中，设置以下环境变量：
export WIFI_SSID="your_wifi"
export WIFI_PASSWORD="your_password"
export GATEWAY_HOST="192.168.2.195"

# sdkconfig.ci 会自动使用这些环境变量
idf.py build
```

### 配置优先级

ESP-IDF 配置系统按以下优先级应用配置（从高到低）：
1. `idf.py menuconfig` 设置的参数
2. 环境变量（通过 `sdkconfig.ci`）
3. `sdkconfig.defaults` 中的默认值

### 恢复默认配置

要恢复默认配置：
```bash
# 删除本地配置
rm sdkconfig sdkconfig.old

# 重新从模板创建
cp sdkconfig.defaults sdkconfig

# 或直接使用 menuconfig
idf.py menuconfig
```

### 高级配置

如需更高级的配置（如 DHCP 替代静态 IP），可直接修改 `main/Kconfig.projbuild` 文件添加新选项。

## 🔒 Git仓库与敏感信息管理

### 保护敏感配置信息

**重要安全提示**：`sdkconfig` 文件包含 WiFi 密码等敏感信息，**绝对不能提交到 Git 仓库**。

本项目已配置 `.gitignore` 文件，自动排除以下敏感文件：
- `sdkconfig` - 包含敏感配置（WiFi密码等）
- `sdkconfig.old` - 旧的配置文件
- `build/` - 编译输出目录
- 其他临时文件和二进制文件

### 新项目初始化流程

首次克隆项目或为新设备配置时，按以下步骤操作：

1. **复制默认配置模板**：
   ```bash
   cp sdkconfig.defaults sdkconfig
   ```

2. **配置项目参数**：
   ```bash
   idf.py menuconfig
   ```
   进入 "OpenClaw Monitor Configuration" 菜单，设置：
   - WiFi SSID 和密码
   - Gateway 主机地址和端口
   - 其他自定义参数

3. **构建和烧录**：
   ```bash
   idf.py build
   idf.py flash
   ```

### 多设备配置管理

如果你需要为不同设备（开发/生产）或不同环境维护不同配置：

1. **创建环境特定的配置文件**：
   ```bash
   cp sdkconfig.defaults sdkconfig.dev
   cp sdkconfig.defaults sdkconfig.prod
   ```

2. **使用特定配置**：
   ```bash
   # 使用开发配置
   cp sdkconfig.dev sdkconfig
   idf.py build flash
   
   # 使用生产配置  
   cp sdkconfig.prod sdkconfig
   idf.py build flash
   ```

3. **将环境配置文件添加到 `.gitignore`**：
   ```
   sdkconfig.*
   !sdkconfig.defaults
   ```

### 团队协作最佳实践

1. **始终提交**：
   - `sdkconfig.defaults`（不含真实密码）
   - `.gitignore`
   - `main/Kconfig.projbuild`

2. **绝不提交**：
   - `sdkconfig`（包含真实密码）
   - 任何包含敏感信息的文件

3. **文档要求**：
   - 在团队文档中记录配置要求
   - 使用密码管理器共享生产密码
   - 为不同环境使用不同密码

### 配置验证

验证配置是否安全：
```bash
# 检查sdkconfig是否在.gitignore中
grep sdkconfig .gitignore

# 检查是否意外添加了敏感文件
git status
```

如果意外提交了敏感文件，立即：
1. 从仓库历史中删除文件
2. 更改所有泄露的密码
3. 重新生成安全密钥

## 🏗️ 项目结构

```
openclaw-monitor/
├── CMakeLists.txt              # 项目根 CMake 配置
├── README.md                   # 本文件
├── main/
│   ├── CMakeLists.txt          # 主组件 CMake 配置
│   ├── Kconfig.projbuild       # menuconfig 配置选项
│   ├── idf_component.yml       # u8g2 库依赖
│   └── openclaw_monitor_main.c # 主程序源码
└── build/                      # 编译输出目录
    ├── openclaw-monitor.bin    # 主程序固件
    ├── bootloader.bin          # Bootloader
    └── partition_table.bin     # 分区表
```

## 🔬 技术栈

- **ESP-IDF v6.0**: Espressif 官方开发框架
- **FreeRTOS**: 实时操作系统
- **u8g2**: 单色显示屏图形库
- **LwIP**: TCP/IP 协议栈
- **ESP32-S2 WiFi**: 802.11 b/g/n 2.4GHz

## 📄 许可证

本项目基于 ESP-IDF 示例代码开发，遵循 Espressif 许可协议。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来改进这个项目。

---

**作者**: Cody (OpenClaw Agent)  
**创建日期**: 2026-02-23  
**ESP-IDF 版本**: v6.0
