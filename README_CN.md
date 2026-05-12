<p align="center">
  <h1 align="center">Smart Environment Monitor</h1>
  <h3 align="center">基于 STM32F103C8T6 的智能环境监测与控制系统</h3>
  <p align="center">
    <img src="https://img.shields.io/badge/MCU-STM32F103C8T6-blue?style=flat-square&logo=stmicroelectronics" alt="MCU">
    <img src="https://img.shields.io/badge/Core-Cortex--M3%20%4072MHz-orange?style=flat-square&logo=arm" alt="Core">
    <img src="https://img.shields.io/badge/Compiler-ARMCLANG%20V6-green?style=flat-square&logo=armkeil" alt="Compiler">
    <img src="https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square" alt="License">
  </p>
</p>

实时嵌入式环境监测与自动控制系统。通过 DHT22、BH1750、PMS5003 传感器采集温湿度、光照强度和 PM2.5 浓度，在 0.96 寸 OLED 屏幕上实时显示数据，并通过滞回控制逻辑自动驱动三路继电器（风扇/加湿器/补光灯）和蜂鸣器报警。支持按键切换自动/手动模式。

---

## 目录

- [功能特性](#功能特性)
- [硬件清单与接线](#硬件清单与接线)
  - [物料清单 (BOM)](#物料清单-bom)
  - [引脚接线图](#引脚接线图)
  - [供电架构](#供电架构)
  - [继电器强电侧接线](#继电器强电侧接线)
  - [推荐的外接设备型号](#推荐的外接设备型号)
- [软件架构](#软件架构)
  - [三层软件模型](#三层软件模型)
  - [数据流管道](#数据流管道)
  - [中断优先级设计](#中断优先级设计)
- [快速开始](#快速开始)
  - [开发环境要求](#开发环境要求)
  - [编译与烧录](#编译与烧录)
  - [首次上电行为](#首次上电行为)
- [配置参数](#配置参数)
  - [全部可配置参数](#全部可配置参数)
  - [滞回控制逻辑详解](#滞回控制逻辑详解)
  - [报警条件](#报警条件)
- [项目目录结构](#项目目录结构)
- [源代码模块详解](#源代码模块详解)
  - [Start 层 -- 启动与 CMSIS](#start-层----启动与-cmsis)
  - [Hardware 层 -- 硬件驱动](#hardware-层----硬件驱动)
  - [Middleware 层 -- 业务逻辑](#middleware-层----业务逻辑)
  - [User 层 -- 应用入口](#user-层----应用入口)
- [关键设计细节](#关键设计细节)
- [已知限制与注意事项](#已知限制与注意事项)
- [版本变更记录](#版本变更记录)
- [许可证](#许可证)

---

## 功能特性

- [x] **DHT22 温湿度传感器** -- 单线 (One-Wire) 数字协议，温度精度 ±0.5 C，湿度精度 ±2% RH
- [x] **BH1750FVI 数字光照传感器** -- 专用 I2C2 总线，1 Lux 高分辨率模式，量程 1~65535 Lux
- [x] **PMS5003 激光 PM2.5 传感器** -- UART 9600bps 主动发送，三状态机逐字节解析 32 字节数据帧
- [x] **两阶段数字滤波** -- 温度/湿度/光照用 8 点滑动平均 (O(1) 更新)，PM2.5 用一阶 IIR 低通滤波 (alpha=0.1)
- [x] **冷启动快速响应** -- 首次收到有效数据时立即填满滤波窗口，避免从零缓慢爬升
- [x] **滞回控制 + 死区** -- 三个继电器各自拥有双阈值和死区，防止临界点反复开关
- [x] **蜂鸣器报警** -- PM2.5 > 150 ug/m3 或温度 > 45 C 时，2kHz PWM 驱动有源蜂鸣器
- [x] **OLED 8 页实时显示** -- 温度/湿度/光照/PM2.5/设备状态/运行模式/运行时间
- [x] **AUTO/MANUAL 模式切换** -- PA0 按键，EXTI 上升沿中断，200ms 软件消抖
- [x] **裸机寄存器编程** -- 所有外设直接操作寄存器，零 HAL/LL 库依赖，代码体积极小
- [x] **双路软件模拟 I2C** -- I2C1 (PB6/PB7 驱动 OLED) 与 I2C2 (PA5/PA6 驱动 BH1750) 物理隔离

---

## 硬件清单与接线

### 物料清单 (BOM)

| 序号 | 模块 | 型号 | 接口 | MCU 引脚 | 备注 |
|------|------|------|------|----------|------|
| 1 | 主控芯片 | STM32F103C8T6 | -- | -- | LQFP48, 64KB Flash, 20KB SRAM |
| 2 | 温湿度传感器 | DHT22 / AM2302 | One-Wire | **PA1** | 需 4.7k 上拉到 3.3V |
| 3 | 光照传感器 | BH1750FVI (GY-302) | I2C2 | **PA5** (SCL), **PA6** (SDA) | ADDR 接 GND, 地址 0x23 |
| 4 | PM2.5 传感器 | PMS5003 / PMS7003 | USART1 | **PA9** (TX), **PA10** (RX) | 9600 8N1, 需 5V 供电 |
| 5 | OLED 显示屏 | SSD1306 0.96" 128x64 | I2C1 | **PB6** (SCL), **PB7** (SDA) | 地址 0x3C |
| 6 | 三路继电器模块 | 5V 低电平触发, 光耦隔离 | GPIO | **PB0** (风扇), **PB1** (加湿器), **PB10** (补光灯) | 10A/250VAC 触点容量 |
| 7 | 有源蜂鸣器模块 | 3.3V 三极管驱动 | TIM2_CH2 PWM | **PB3** | 2kHz 方波, 50% 占空比 |
| 8 | 轻触按键 | 6x6mm 四脚 | EXTI0 | **PA0** | 需外接 4.7k 上拉到 3.3V |
| 9 | I2C 上拉电阻 | 4.7k x4 | -- | PA5, PA6, PB6, PB7 | I2C 总线必备 |
| 10 | DHT22 上拉电阻 | 4.7k x1 | -- | PA1 | One-Wire 总线必备 |
| 11 | 去耦电容 | 100nF x3 | -- | VDD 引脚 | MCU 电源旁路 |

### 引脚接线图

```
                          STM32F103C8T6 (LQFP48 封装)
                       +-------------o-------------+
  按键 ───────────────| PA0  (10)                 |
  DHT22 DATA ─────────| PA1  (11)    [4.7k上拉]    |
  BH1750 SCL ─────────| PA5  (15)    [4.7k上拉]    |
  BH1750 SDA ─────────| PA6  (16)    [4.7k上拉]    |
  PMS5003 RXD ────────| PA9  (32)                 |
  PMS5003 TXD ────────| PA10 (33)                 |
  ST-Link SWDIO ──────| PA13 (36)                 |
  ST-Link SWCLK ──────| PA14 (37)                 |
  继电器 IN1 (风扇) ──| PB0  (18)                 |
  继电器 IN2 (加湿) ──| PB1  (19)                 |
  蜂鸣器 I/O ─────────| PB3  (39)    TIM2_CH2 PWM |
  OLED SCL ───────────| PB6  (42)    [4.7k上拉]    |
  OLED SDA ───────────| PB7  (43)    [4.7k上拉]    |
  继电器 IN3 (补光) ──| PB10 (21)                  |
                       |                           |
  3.3V ────────────────| VDD (9, 24, 48)           |
  GND ─────────────────| VSS (8, 23, 47)           |
  GND ─────────────────| BOOT0 (44)                |
                       +---------------------------+
```

**注意事项：**

- 所有 I2C 引脚 (PA5/PA6/PB6/PB7) 和 DHT22 DATA (PA1) 必须外接 4.7k 上拉电阻到 3.3V，否则通信失败。
- PA13/PA14 兼作 SWD 调试接口，接线时保留这两个引脚给 ST-Link。
- JTAG 接口已在固件中关闭（保留 SWD），PB3/PB4/PA15 释放为普通 GPIO。

### 供电架构

```
+5V 电源轨 (外部供电)
  ├── 继电器模块 VCC (线圈供电，5V)
  └── PMS5003 VCC (传感器加热器+风扇，5V)

+3.3V 电源轨 (LDO 由 5V 降压，或 ST-Link 提供)
  ├── STM32F103C8T6 VDD (全部 3 个引脚)
  ├── DHT22 VCC
  ├── BH1750 VCC
  ├── OLED VCC
  ├── 蜂鸣器 VCC
  ├── I2C 上拉电阻 (PA5/PA6/PB6/PB7)
  └── 按键上拉电阻 (PA0)
```

**要点：** MCU 每个 VDD 引脚旁应放置 100nF 去耦电容到 GND。继电器线圈和 MCU 使用不同电压轨（5V vs 3.3V），通过光耦隔离。

### 继电器强电侧接线

继电器的 **IN1/IN2/IN3（信号输入端）** 通过杜邦线连接到面包板，再连接到 MCU 的 PB0/PB1/PB10。这是低压信号路径。

继电器的 **COM/NO 端子** 直接连接 220V 用电设备，**不要经过面包板**：

```
220V 火线 L → COM1 → [继电器内部触点] → NO1 → 风扇 → 220V 零线 N
220V 火线 L → COM2 → [继电器内部触点] → NO2 → 加湿器 → 220V 零线 N
220V 火线 L → COM3 → [继电器内部触点] → NO3 → 补光灯 → 220V 零线 N
```

**工作原理：**
- MCU PBx 输出**低电平** (0V) → 光耦导通 → 三极管驱动 → 继电器线圈通电 → 触点吸合 → COM 与 NO 导通 → 设备得电工作
- MCU PBx 输出**高电平** (3.3V) → 光耦截止 → 继电器线圈失电 → 触点断开 → 设备断电

**安全提醒：** 继电器模块上的光耦提供了 3.3V 低压侧和 220V 高压侧之间的电气隔离。接线时确保强电端子螺丝拧紧，裸露部分用电工胶带包裹。

### 连接外接设备型号

这三个 220V 设备通过继电器控制，需自行购买：

| 设备 | 推荐型号 | 规格 | 参考价格 |
|------|---------|------|----------|
| 风扇 | DP200A 2123XBL | 120x120x38mm, 220V, 0.12A, 静音 | ~20 元 |
| 加湿器 | USB 雾化模块 + 220V→USB 适配器 | 超声波雾化片+驱动板, 适合水杯场景 | ~10 元 |
| 补光灯 | E27 LED 全光谱植物灯泡 | 220V, 5W, E27 螺口, 红蓝光谱 | ~15 元 |

> 风扇也可用更小的 80mm 8025 系列 (220V)；加湿器可用成品桌面加湿器 (~30 元)；补光灯普通 LED 灯泡也可用，但植物补光灯光谱更适合。

---

## 软件架构

### 三层软件模型

项目采用严格的三层架构，层级间单向依赖（上层依赖下层，下层不感知上层）：

```
+--------------------------------------------------+
|                  User 层（应用入口）                 |
|  main.c    main.h    delay.c    systick.c          |
|  职责：系统初始化流程编排、主循环调度、全局配置参数     |
+--------------------------------------------------+
|                Middleware 层（业务逻辑）              |
|  sensor.c    filter.c    control.c    display.c    |
|  职责：传感器统一采集+容错缓存、数字滤波算法、         |
|        滞回控制决策+按键模式切换、OLED 界面渲染        |
+--------------------------------------------------+
|                Hardware 层（硬件驱动）                |
| i2c.c  i2c2.c  dht22.c  bh1750.c  uart.c          |
| pm25.c  oled.c  alarm.c                            |
|  职责：纯硬件操作（寄存器读写、通信协议实现），         |
|        不包含任何业务逻辑                             |
+--------------------------------------------------+
|                Start 层（CMSIS + 启动）              |
|  startup_stm32f10x_hd.s    system_stm32f10x.c      |
|  职责：向量表、栈/堆初始化、HSE+PLL 72MHz 时钟树配置   |
+--------------------------------------------------+
```

**设计原则：**

- Hardware 层：只关心寄存器和通信协议，对外暴露简洁的 `Init/Read/Write` 接口
- Middleware 层：封装业务逻辑，调用 Hardware 层接口，依赖 `main.h` 中的配置参数
- User 层：系统入口和调度中心，组合 Middleware 各模块
- **所有可调参数集中在 `main.h`**，修改阈值/引脚/地址无需跨文件搜索
- 模块间全局状态用 `static` 文件作用域变量限制访问，通过函数接口操作

### 数据流管道

```
  DHT22 ──┐
  BH1750 ─┼──> sensor.c ──> filter.c ──┬──> control.c ──> 继电器 x3
  PMS5003 ─┘    (采集+缓存)   (平滑滤波)  │
                                        ├──> alarm.c ────> 蜂鸣器
                                        │
                                        └──> display.c ──> OLED
```

详细的逐阶段数据流见下方 [User 层 -- 主循环](#主循环-mainc) 章节。

### 中断优先级设计

| 中断源 | NVIC 通道 | 抢占优先级 | 触发频率 | 任务 |
|--------|----------|-----------|---------|------|
| SysTick | 内核 | 0 (最高) | 1kHz | 递增 `g_sysTick`，每 1000 次置 `g_sample_flag` |
| USART1 | 37 | 1 | 不定 (每字节) | 接收 PMS5003 数据写入 64 字节环形缓冲区 |
| EXTI0 | 6 | 2 | 按键释放时 | 200ms 软件消抖后切换 AUTO/MANUAL 模式 |

优先级分组 = 3（4 位抢占优先级 + 0 位子优先级）。

**环形缓冲区无锁机制：** `rx_head` 仅由 USART1 ISR 修改，`rx_tail` 仅由主循环修改，构成单生产者-单消费者模型，无需临界区保护。

**DHT22 时序保护：** 整个约 5ms 通信期间调用 `__disable_irq()` / `__enable_irq()` 关闭全局中断，防止时序被 SysTick 或 USART1 中断打断。

---

## 快速开始

### 开发环境要求

- **Keil MDK-ARM V5** 集成开发环境（需 ARMCLANG V6.22 或更高版本）
- **STM32F1xx DFP** 器件家族包（v2.4.1 或更高）
- **ST-Link/V2** 或兼容的 SWD 调试/烧录器
- 按 [物料清单](#物料清单-bom) 准备的硬件模块

### 编译与烧录

```bash
# 克隆仓库
git clone https://github.com/<your-username>/smart-env-monitor.git
cd smart_env

# 方式一：Keil IDE 图形界面
# 打开 Environment.uvprojx, 选择 "Target 1", 按 F7 编译, 按 F8 烧录

# 方式二：命令行编译
UV4.exe -b Environment.uvprojx -j0 -t "Target 1" -o build.log
```

**烧录器接线：** ST-Link 的 SWDIO 接 PA13 (36脚)，SWCLK 接 PA14 (37脚)，3.3V 和 GND 对应接入。

> 固件中 JTAG 已关闭（仅保留 SWD），烧录时 ST-Link 自动使用 SWD 协议，无需额外设置。

### 首次上电行为

1. 系统上电 → 启动文件自动调用 `SystemInit()` 配置 72MHz 时钟 → 进入 `main()`
2. 初始化全部外设（I2C → UART → 继电器 → 蜂鸣器 → 传感器 → OLED → 滤波器 → SysTick）
3. OLED 显示 "System Starting..." 持续 1 秒后清屏
4. PMS5003 风扇启动，约 30 秒后数据稳定（预热期间 PM2.5 显示 `---`）
5. DHT22 首次采集，滤波器进入冷启动快速填充模式
6. 主循环以 1 秒周期运行：采集 → 滤波 → 控制 → 报警 → 显示
7. 按键可在任何时刻短按切换 AUTO/MANUAL 模式

---

## 配置参数

### 全部可配置参数

所有阈值、引脚定义、地址、滤波参数集中在 **`User/main.h`** 中，修改一个宏即可调整系统行为：

| 类别 | 宏名称 | 默认值 | 含义 |
|------|--------|--------|------|
| **系统** | `SAMPLE_INTERVAL_MS` | 1000 | 主循环采样间隔 (ms) |
| **滤波** | `FILTER_WINDOW_SIZE` | 8 | 滑动平均窗口样本数 |
| **滤波** | `IIR_ALPHA` | 0.1f | IIR 滤波系数 (0=最平滑, 1=不过滤) |
| **风扇** | `TEMP_HIGH_THRESHOLD` | 30.0f | 温度高于此值开风扇 |
| **风扇** | `TEMP_LOW_THRESHOLD` | 26.0f | 温度低于此值关风扇 |
| **加湿器** | `HUMI_LOW_THRESHOLD` | 40.0f | 湿度低于此值开加湿器 |
| **加湿器** | `HUMI_HIGH_THRESHOLD` | 60.0f | 湿度高于此值关加湿器 |
| **补光灯** | `LIGHT_LOW_THRESHOLD` | 100.0f | 光照低于此值开补光灯 |
| **补光灯** | `LIGHT_HIGH_THRESHOLD` | 500.0f | 光照高于此值关补光灯 |
| **报警** | `PM25_ALARM_THRESHOLD` | 150 | PM2.5 高于此值触发蜂鸣器 (ug/m3) |
| **报警** | `TEMP_CRITICAL` | 45.0f | 温度高于此值触发蜂鸣器 ( C) |
| **继电器** | `RELAY_FAN_PIN` | `GPIO_ODR_ODR0` | 风扇继电器位掩码 (PB0) |
| **继电器** | `RELAY_HUMIDIFIER_PIN` | `GPIO_ODR_ODR1` | 加湿器继电器位掩码 (PB1) |
| **继电器** | `RELAY_LIGHT_PIN` | `GPIO_ODR_ODR10` | 补光灯继电器位掩码 (PB10) |
| **I2C** | `I2C_SCL_PIN` / `I2C_SDA_PIN` | 6 / 7 | I2C1 引脚 (PB6/PB7) |
| **I2C2** | `I2C2_SCL_PIN` / `I2C2_SDA_PIN` | 5 / 6 | I2C2 引脚 (PA5/PA6) |
| **UART** | `PM25_RX_BUF_SIZE` | 64 | PM2.5 接收环形缓冲区大小 |

### 滞回控制逻辑详解

每个被控设备有两个阈值，中间是**死区（dead zone）**。死区内传感器数值波动不会导致继电器状态变化，这是防止继电器在阈值临界点反复吸合/断开（抖动）的关键设计：

```
风扇 (温度控制):
  温度 > 30.0 C  -->  开风扇
  温度 < 26.0 C  -->  关风扇
  26.0 ~ 30.0 C  -->  保持原状态不变  (4 C 死区)

加湿器 (湿度控制):
  湿度 < 40.0%   -->  开加湿器
  湿度 > 60.0%   -->  关加湿器
  40.0% ~ 60.0%  -->  保持原状态不变  (20% 死区)

补光灯 (光照控制):
  光照 < 100 Lux -->  开补光灯
  光照 > 500 Lux -->  关补光灯
  100 ~ 500 Lux  -->  保持原状态不变  (400 Lux 死区)
```

**实现细节：** 每次 `ControlDevice()` 调用时，先检查当前继电器状态（`status` 变量），仅在传感器值**穿过阈值且状态与目标不同**时才操作 ODR 寄存器。这避免了每周期重复写 GPIO 寄存器，即使传感器值在死区内大幅波动也完全不影响输出。

补光灯的死区 (400 Lux) 比风扇 (4 C) 和加湿器 (20%) 大得多，是因为光照受外界（窗户、云层、室内灯光）影响波动远比温湿度剧烈。

### 报警条件

```c
if (pm2_5 > 150 ug/m3)  触发蜂鸣器;  // PM2.5 超标
if (temp > 45.0 C)       触发蜂鸣器;  // 设备过热保护
```

任一条件满足即触发。两个条件都不满足时蜂鸣器关闭。报警判断在滤波之后执行，避免传感器瞬时噪声引起误报。

---

## 项目目录结构

```
smart_env/
|
|-- Start/                            CMSIS 核心支持 + 启动文件
|   |-- startup_stm32f10x_hd.s        向量表 (含所有中断入口)、RESET 复位流程、
|   |                                 栈顶初始化 (0x400B)、堆初始化 (0x200B)
|   |-- system_stm32f10x.c            SystemInit() → SetSysClockTo72()
|   |                                 HSE(8MHz) → PLL×9 → 72MHz
|   |-- core_cm3.c / .h               Cortex-M3 内核函数
|   |                                 (NVIC_SetPriority, __WFI, __disable_irq等)
|   |-- stm32f10x.h                   STM32F10x 全系列外设寄存器位定义宏
|   |                                 (GPIO_CRL_MODE0_0 ... TIM_CCER_CC2E 等)
|
|-- User/                             应用入口层
|   |-- main.c                        系统初始化流程、主循环 (A→E 五阶段)
|   |-- main.h                        ★ 全部配置参数的唯一入口
|   |                                 (阈值/引脚/地址/数据结构/枚举类型)
|   |-- delay.c / .h                  微秒/毫秒/秒延时
|   |                                 (Delay_us 保存/恢复 SysTick 配置)
|   |-- systick.c / .h                1ms 系统时基 ISR + 采集分频
|
|-- Hardware/                         硬件驱动层 (每个子目录一个外设模块)
|   |-- I2C/
|   |   |-- i2c.c / .h               软件模拟 I2C1 (PB6=SCL, PB7=SDA, OLED 专用)
|   |   |-- i2c2.c / .h              软件模拟 I2C2 (PA5=SCL, PA6=SDA, BH1750 专用)
|   |-- BH1750/
|   |   |-- bh1750.c / .h            光照传感器驱动 (I2C2 通信, 连续高分辨率模式)
|   |-- DHT22/
|   |   |-- dht22.c / .h             温湿度传感器驱动 (One-Wire 单线协议, PA1)
|   |-- PM25/
|   |   |-- pm25.c / .h              PMS5003 驱动 (UART 三状态帧解析状态机)
|   |-- UART/
|   |   |-- uart.c / .h              USART1 串口驱动 (9600 8N1, 64B 环形缓冲区)
|   |-- OLED/
|   |   |-- oled.c / .h              SSD1306 驱动 (128x64, 6x8 ASCII 字库, 页寻址)
|   |-- Alarm/
|       |-- alarm.c / .h             有源蜂鸣器驱动 (TIM2_CH2 PWM 2kHz, PB3)
|
|-- Middleware/                       业务逻辑层 (调用 Hardware 层，不访问寄存器)
|   |-- Sensor/
|   |   |-- sensor.c / .h            三传感器统一采集 + 数据缓存容错
|   |-- Filter/
|   |   |-- filter.c / .h            数字滤波 (8点滑动平均 + 一阶 IIR 低通)
|   |-- Control/
|   |   |-- control.c / .h           滞回控制 + 按键 ISR 模式切换 + 软件消抖
|   |-- Display/
|       |-- display.c / .h           OLED 8 页界面布局渲染
|
|-- Environment.uvprojx               Keil MDK-ARM 工程文件
|-- Objects/                          编译产物 (.o / .d / .axf / .sct)
|-- Listings/                         链接器输出 (.map 符号表 / 反汇编)
|-- DebugConfig/                      Keil 调试器目标配置快照
```

---

## 源代码模块详解

### Start 层 -- 启动与 CMSIS

#### startup_stm32f10x_hd.s

汇编启动文件，包含以下关键部分：

- **向量表 (Vector Table)**：存放初始 SP 值（栈顶 0x400 字节）和所有中断/异常处理函数入口地址。未实现的 ISR 弱定义 (`[WEAK]`) 为死循环 `B .`，用户定义同名函数后自动覆盖。
- **Reset_Handler**：芯片上电/复位后首先执行的代码。流程为：
  ```
  SP = __initial_sp
  → 调用 SystemInit()     // 配置 72MHz 时钟
  → 调用 __main           // C 运行时初始化 (bss 清零 / data 段搬运)
  → 跳转到 main()         // 进入用户代码
  ```
- **栈大小**：0x400 (1024 字节)
- **堆大小**：0x200 (512 字节)

#### system_stm32f10x.c

时钟树初始化。`SystemInit()` 在进入 `main()` 之前由启动代码调用，无需在用户代码中再次配置时钟：

```
SystemInit():
  → 开启 HSI (内部 8MHz 备援)
  → 复位 RCC 所有配置寄存器
  → 设置 Flash 2 等待周期 + 预取缓冲使能 (72MHz 下必须)
  → SetSysClockTo72():
       HSE(8MHz 外部晶振) 开启 → 等待稳定
       PLLSRC=HSE, PLLMULL=9 → PLL 输出 72MHz
       HPRE=/1 (HCLK=72MHz), PPRE1=/2 (PCLK1=36MHz), PPRE2=/1 (PCLK2=72MHz)
       SW=PLL → 系统时钟切换到 72MHz
  → 等待 SWS 确认切换完成
```

#### stm32f10x.h

STM32F10x 系列全外设寄存器位定义宏，本项目所有寄存器操作均使用此文件中的宏：
- GPIO: `GPIO_CRL_MODE0_0`, `GPIO_CRH_CNF10_1`, `GPIO_ODR_ODR0` 等
- RCC: `RCC_APB2ENR_IOPBEN`, `RCC_APB1ENR_TIM2EN` 等
- TIM: `TIM_CCMR1_OC2M_1`, `TIM_CCER_CC2E`, `TIM_CR1_CEN` 等
- AFIO: `AFIO_MAPR_SWJ_CFG`, `AFIO_EXTICR1_EXTI0` 等
- EXTI: `EXTI_IMR_MR0`, `EXTI_RTSR_TR0`, `EXTI_PR_PR0` 等
- USART: `USART_CR1_UE`, `USART_SR_RXNE` 等

### Hardware 层 -- 硬件驱动

#### I2C (i2c.c / i2c2.c) -- 双路软件模拟 I2C

**为什么不用硬件 I2C：** STM32F1 系列的硬件 I2C 外设有文档记载的总线锁死缺陷（当从机异常拉死 SDA 时，硬件状态机可能卡死无法恢复）。软件模拟 I2C 更稳定可靠，且完全不依赖硬件 I2C 外设。

**两路独立 I2C 的设计背景：** 面包板环境下，无法用杜邦线将 BH1750 和 OLED 两个 I2C 设备并联到同一组 MCU 引脚（物理上难以实现 T 型分支）。因此为 BH1750 单独分配 PA5/PA6 实现独立的第二路 I2C 总线。

| 总线 | 引脚 | 驱动文件 | 用途 | 从机地址 |
|------|------|---------|------|---------|
| I2C1 | PB6 (SCL), PB7 (SDA) | i2c.c | OLED 显示屏 | 0x3C |
| I2C2 | PA5 (SCL), PA6 (SDA) | i2c2.c | BH1750 光照传感器 | 0x23 |

**关键实现细节：**

- GPIO 配置为**开漏输出** (CNF=01, MODE=11, 50MHz)：写 ODR=0 使 NMOS 导通拉低总线；写 ODR=1 使 NMOS 截止，由外部 4.7k 上拉电阻拉高总线。开漏输出天然支持 I2C 的"线与"特性（多个设备可同时驱动总线而不会短路）。
- 用 **BSRR 寄存器**操作 GPIO：低 16 位写 1 置位 ODR（输出高电平），高 16 位写 1 复位 ODR（输出低电平）。BSRR 提供原子 IO 操作，无需读-改-写序列，避免中断打断导致的竞态条件。
- I2C 时序：每个位操作间 `Delay_us(5)` 延时 5 微秒，保证 SCL 约 100kHz 标准模式下的信号建立/保持时间。
- 读字节时 SDA 释放由主机控制（`SDA_H()`），由从机输出数据位。

**I2C 协议函数：**

| 函数 | 功能 |
|------|------|
| `I2C_Init()` / `I2C2_Init()` | 配置 GPIO 开漏输出，释放总线 |
| `I2C_Start()` / `I2C2_Start()` | 发送起始条件 (SCL 高时 SDA 下降) |
| `I2C_Stop()` / `I2C2_Stop()` | 发送停止条件 (SCL 高时 SDA 上升) |
| `I2C_WriteByte()` / `I2C2_WriteByte()` | 写一个字节 (MSB 优先)，返回 ACK/NACK |
| `I2C_ReadByte()` / `I2C2_ReadByte()` | 读一个字节 (MSB 优先)，ACK/NACK 由调用者发送 |
| `I2C_WaitAck()` / `I2C2_WaitAck()` | 等待从机应答 (第 9 个 SCL 脉冲期间读 SDA) |
| `I2C_SendAck()` / `I2C2_SendAck()` | 主机发送应答 (ACK=0 继续，NACK=1 停止) |

#### BH1750 (bh1750.c) -- 数字光照传感器

- **I2C 地址**：7 位地址 0x23 (ADDR 引脚接 GND)，写地址=0x46，读地址=0x47
- **初始化**：
  1. 发送上电命令 (0x01) → 等待 10ms 电路稳定
  2. 发送连续高分辨率模式命令 (0x10) → 等待 180ms 首次测量完成
- **读取流程**：
  ```
  I2C2_Start → 发送 0x47 (读地址) → 检查 ACK
    → 读高字节 (I2C2_ReadByte + ACK)
    → 读低字节 (I2C2_ReadByte + NACK)
    → I2C2_Stop
  ```
- **计算公式**：`Lux = raw / 1.2`（手册规定的 ADC 系数）
- **错误处理**：通信失败（从机 NACK）→ 返回 -1.0f。上层 `sensor.c` 中通过 `light >= 0.0f` 判断有效性，失败时保留上次有效值。
- **模式选择**：本系统使用 0x10（连续高分辨率模式，1 Lux 精度，~120ms 测量时间）。0x11 可获 0.5 Lux 精度但测量时间翻倍。0x13 为低分辨率模式 (4 Lux, ~16ms) 适合高响应速度场景。

#### DHT22 (dht22.c) -- 数字温湿度传感器

- **GPIO 配置**：PA1 开漏输出 (CNF=01, MODE=11)，利用开漏输出的特性实现单线双向通信（写 0 拉低，写 1 靠 4.7k 上拉电阻拉高，切换为输入时读 IDR）。
- **BSRR 宏**：`DHT22_H()` 写 BSRR 低 16 位置高，`DHT22_L()` 写 BSRR 高 16 位置低，`DHT22_IN()` 读 IDR 获取当前电平。
- **通信流程** (全程约 5ms，期间 `__disable_irq()` 保护时序)：

  **阶段 1 -- MCU 发起始信号：**
  ```
  MCU 拉低 1200us (>1ms 要求) → 释放 30us (20~40us 窗口)
  ```
  **阶段 2 -- DHT22 响应：**
  ```
  等待 DHT22 拉低 80us → 等待拉高 80us → 等待拉低 (准备发数据)
  每步超时 1000 次循环 (~1ms)，超时则恢复中断并返回 valid=0
  ```
  **阶段 3 -- 接收 40 位数据 (5 字节 × 8 位)：**
  ```
  每位格式: 50us 低电平 + (26~28us 高=bit0 / 70us 高=bit1)
  解码策略: 等待低电平结束 → 延时 40us → 读 GPIO 电平
  bit0: 延时 40us 后高电平已回落 → 读到 0
  bit1: 延时 40us 后高电平仍在 → 读到 1
  等待剩余高电平结束 → 进入下一位
  ```
  **阶段 4 -- 校验：**
  ```
  data[4] == (data[0] + data[1] + data[2] + data[3]) 的低 8 位
  通过 → valid=1, 湿度 = (data[0]<<8|data[1]) * 0.1%RH
          温度 = (data[2]<<8|data[3]) * 0.1 C
  ```
- **物理限制**：传感器采样间隔 >= 2 秒。当前 1 秒采集策略依赖校验和拒绝未就绪的返回数据。

#### PMS5003 (pm25.c) + UART (uart.c) -- 激光 PM2.5 传感器

**UART 层 (uart.c)：**

- **引脚**：PA9 (TX, 复用推挽), PA10 (RX, 浮空输入)
- **波特率**：9600 8-N-1，BRR = 72MHz / (16 × 9600) = 468.75 → 0x1D4C
- **环形缓冲区 (64 字节)**：
  - 头指针 `rx_head`：USART1 ISR 写入位置 (volatile, 仅 ISR 修改)
  - 尾指针 `rx_tail`：主循环读取位置 (volatile, 仅主循环修改)
  - 空判断：`head == tail`
  - 满判断：`(head + 1) % 64 == tail` (保留一个空位)
  - 溢出保护：缓冲区满时丢弃新字节保护已缓冲数据；USART ORE 溢出错误通过读 DR+SR 清除
- **printf 重定向**：实现 `fputc()` 函数，可将 `printf()` 输出重定向到串口供调试

**PM2.5 帧解析层 (pm25.c)：**

PMS5003 以 200~800ms 间隔主动发送 32 字节数据帧（无需 MCU 请求），格式如下：

| 偏移 | 长度 | 内容 |
|------|------|------|
| 0 | 1 | 帧头 1 = 0x42 |
| 1 | 1 | 帧头 2 = 0x4D |
| 2~3 | 2 | 帧长度 (固定 0x001C = 28) |
| 4~5 | 2 | PM1.0 标准浓度 (大端) |
| 6~7 | 2 | PM2.5 标准浓度 (大端) |
| 8~9 | 2 | PM10 标准浓度 (大端) |
| 10~15 | 6 | 大气环境浓度 (PM1.0/2.5/10) |
| 16~29 | 14 | 颗粒物计数 (0.3/0.5/1.0/2.5/5.0/10um) |
| 30~31 | 2 | 前 30 字节累加和 (大端, 低 16 位) |

**三状态解析状态机：**

```
                    +--(收到 0x42)--+
                    |               |
                    v               |
  IDLE ──(0x42)──> GOT_42 ──(0x4D)──> FRAME ──(收满 32 字节)──> 校验
   ^                |   ^                            |
   |   (非 0x42     |   |    (校验失败)              |
   |    非 0x4D)    |   +----(0x42)----+             |
   +----<-----------+                  +-------------+
```

**鲁棒性设计：**
- 逐字节扫描，不假设帧边界对齐（可从上电乱码中自动同步）
- 处理连续帧头：`GOT_42` 状态下再次收到 `0x42`，重新开始而非丢弃（可能前一帧损坏）
- 双层校验：帧长度必须 = 28 + 前 30 字节累加和匹配
- 校验失败整帧丢弃回到 IDLE，不会因一个错位导致后续所有帧损坏

#### OLED (oled.c) -- SSD1306 128x64 显示驱动

- **I2C 地址**：7 位 0x3C，写地址 0x78
- **GDDRAM 组织**：128 列 × 64 行 = 8 页 (Page0~7) × 128 段。每页 8 像素高，每列 1 字节，LSB 对应页内顶部像素，MSB 对应底部像素。
- **初始化命令序列 (31 条)**：关显示 → 页寻址模式 → COM 扫描方向 → 对比度=255 → **电荷泵使能** (0x8D,0x14, 3.3V 屏必须!) → 段重映射 → 正常显示 → 多路复用比=64 → 时钟分频 → 预充电周期 → COM 硬件配置 → VCOMH 电压 → 开显示
- **字库**：`font6x8[95][6]`，覆盖 ASCII 0x20~0x7E，每字符 6 像素宽 (5 有效 + 1 间距)。每行最多 128/6=21 个字符。
- **像素残影清除 (`OLED_ClearLine`)**：显示前遍历指定页面从写入列到行末写 0x00。解决了数值变短时旧像素残留的视觉 bug（如 "125.0" → "5.0"，若不清除旧的 "12" 像素仍可见）。

#### Alarm (alarm.c) -- 蜂鸣器 PWM 驱动

- **引脚**：PB3 默认功能 JTDO → 需通过 AFIO 重映射关闭 JTAG（保留 SWD）释放为 GPIO
- **AFIO 重映射配置**：`AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG; AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;`（SWJ_CFG=010：关闭 JTAG-DP，保留 SW-DP）
- **TIM2_CH2 PWM 参数**：
  ```
  PSC = 71    → 定时器计数频率 = 72MHz / (71+1) = 1MHz
  ARR = 499   → PWM 频率 = 1MHz / (499+1) = 2kHz
  CCR2 = 250  → 占空比 = 250/500 = 50% (报警)
  CCR2 = 0    → 占空比 = 0% (但输出极性反转后实际为高电平 = 静音)
  ```
- **极性反转 (CC2P=1)**：蜂鸣器模块为低电平触发（I/O 低电平 → 三极管导通 → 蜂鸣器响）。设置 `TIM_CCER_CC2P` 反转输出极性后，CCR2=0 时输出恒高 → 蜂鸣器静音；CCR2=250 时 50% 占空比低电平 → 蜂鸣器响。

### Middleware 层 -- 业务逻辑

#### Sensor (sensor.c) -- 传感器统一采集与容错

- **设计模式**：门面模式 (Facade) -- 对外暴露统一的 `Sensor_Collect()` 和 `GetTemperature()/GetHumidity()/GetLight()/GetPM25()` 接口，内部聚合三个传感器驱动。调用方无需感知底层传感器类型。
- **容错策略**：每个传感器读取后检查 valid 标志，读取失败时**保留上次有效缓存值**，避免单次故障传播错误数据到后续模块。
- **缓存初始值**：温度/湿度 = -999.0，光照 = -1.0，PM2.5 = -1。这些"不可能值"用于上层判断"从未读取成功"（如 Display 层据此显示 `---`）。

#### Filter (filter.c) -- 数字滤波

**滑动平均滤波 (温度/湿度/光照)：**

- 窗口大小 = 8 (`FILTER_WINDOW_SIZE`)
- **O(1) 更新算法**：维护窗口内数据总和 `sum`，每次更新时 `sum = sum - 被淘汰的旧值 + 新值`，避免传统滑动平均的 O(N) 遍历求和时间开销。这使得 8 点窗口和 100 点窗口的计算成本完全相同。
- **冷启动优化** (`MAFilter_Fill`)：窗口为空时，立即将所有 8 个历史值设为当前值。若不这样做，首次读到 25.5 C 时滤波输出为 25.5/8 = 3.2 C（被初始的 0 拉低平均值），需要 8 次采样 (8 秒) 才能爬升到真实值。Fill 之后首次输出就是真实值。

**IIR 低通滤波 (PM2.5)：**

- 公式：`y(n) = alpha × x(n) + (1 - alpha) × y(n-1)`，其中 alpha = 0.1 (`IIR_ALPHA`)
- 新数据权重仅 10%，旧数据权重 90%，实现强平滑。适合 PM2.5 数值波动大但变化趋势相对缓慢的特点。
- 响应速度：约需 22 次采样 = 22 秒达到 90% 稳态响应 (时间常数 tau = 1/alpha = 10 次)。
- 冷启动：`g_pm25_inited == 0` 时直接赋值（`y = x`），避免从 0 开始缓慢爬升。

#### Control (control.c) -- 滞回控制 + 按键模式切换

**继电器控制：**

- GPIO 初始化：PB0/PB1/PB10 推挽输出 (CNF=00, MODE=11, 50MHz)。初始化时 ODR 写 1 (高电平)，保证所有继电器上电瞬间处于断开状态（安全设计）。
- 低电平触发逻辑：`Relay_On(PIN)` → `GPIOB->ODR &= ~PIN` (写 0 = 吸合)；`Relay_Off(PIN)` → `GPIOB->ODR |= PIN` (写 1 = 断开)。
- 滞回实现 (`ControlDevice`)：每次被 `ControlAll()` 调用时检查每个设备。仅在状态需要改变时才写 ODR 寄存器（`*status == 0` 或 `*status == 1` 的前置判断）。
- MANUAL 模式：`g_systemMode == MODE_MANUAL` 时函数直接 return，跳过所有自动控制逻辑。

**按键 ISR (EXTI0_IRQHandler)：**

- 触发条件：PA0 上升沿（按键松开时），EXIT0 中断
- 软件消抖：`(g_sysTick - last_tick) > 200ms` 条件过滤机械触点抖动（通常 5-50ms）
- 切换逻辑：`g_systemMode` AUTO <-> MANUAL
- 为什么用上升沿（松开触发）而非下降沿（按下触发）：用户按下时长不计入消抖窗口，操作体验更好

#### Display (display.c) -- OLED 界面渲染

8 页 (Page0~7) 布局，每页 8 像素高：

| 页 | 内容 | 示例 | 说明 |
|----|------|------|------|
| 0 | 温度 | `Temp:25.5C` | 保留 1 位小数，无效显示 `---` |
| 1 | 湿度 | `Humi:55.2%` | 保留 1 位小数，无效显示 `---` |
| 2 | 光照 | `Lux :320` | 整数显示，无效显示 `---` |
| 3 | PM2.5 | `PM2.5:35 ug` | 整数显示，无效显示 `---` |
| 4 | 风扇/加湿器状态 | `Fan:ON Hum:OFF` | 两位掩码解码自 `g_deviceStatus` |
| 5 | 补光灯/模式 | `Lit:OFF Mod:AUTO` | 模式显示 AUTO 或 MANU |
| 6 | 标题线 | `---Smart Env Monitor---` | 固定内容 |
| 7 | 运行时间 | `Run:3600s` | `g_sysTick / 1000` 秒 |

**渲染流程**：先显示标签文字 → `OLED_ClearLine()` 擦除数值区域的旧像素 → 显示新数值。每页刷新前执行清除操作，确保不会出现像素残影。

### User 层 -- 应用入口

#### 主循环 (main.c)

**初始化顺序 (必须严格遵守)：**

```
1. I2C_Init() + I2C2_Init()    // PB6/PB7, PA5/PA6 开漏输出 (需先于 BH1750/OLED)
2. UART_Init()                 // PA9/PA10 + USART1 9600 8-N-1, NVIC 优先级=1
3. Control_Init()              // PB0/PB1/PB10 推挽高 (继电器断开) + PA0 EXTI0
4. Alarm_Init()                // PB3 AFIO 重映射 + TIM2 PWM 2kHz
5. Sensor_Init()               // 调用 DHT22/BH1750/PM25 各自的 Init
6. Display_Init()              // OLED 31 条初始化命令
7. Filter_Reset()              // 清零所有滤波器状态
8. SysTick_Init()              // 最后启动! LOAD=71999, 1ms 中断使能
9. 启动画面 + 首次采集预热
```

**SysTick 最后启动的原因**：初始化过程中 `Delay_us()` 会临时占用 SysTick 外设。如果在 SysTick 启动后再大量调用 I2C/DHT22 延时，虽然 delay.c 保存/恢复机制能保证不永久丢失中断，但会导致 SysTick 累积延迟。

**主循环 (每 1 秒执行一次，由 `g_sample_flag` 驱动)：**

```
while (1) {
    if (g_sample_flag) {
        g_sample_flag = 0;              // 立即清零, 防止重入

        (A) Sensor_Collect();            // 采集三传感器原始数据
            temp_raw  = GetTemperature();
            humi_raw  = GetHumidity();
            light_raw = GetLight();
            pm25_raw  = GetPM25();

        (B) Filter:                      // 数字滤波
            temp_filtered  = FilterTemperature(temp_raw);
            humi_filtered  = FilterHumidity(humi_raw);
            light_filtered = FilterLight(light_raw);
            pm25_filtered  = FilterPM25(pm25_raw);

        (C) ControlAll(...);             // 滞回控制 (仅在 AUTO 模式生效)
            g_deviceStatus = Control_GetStatus();

        (D) CheckAlarm(...)              // PM2.5 > 150 或 temp > 45 C?
            ? Alarm_On() : Alarm_Off();

        (E) DisplayAll(...);             // 刷新 OLED 8 页内容
    }
    __WFI();  // 休眠, 任意中断唤醒
}
```

#### 全局配置 (main.h)

所有可调阈值、引脚位掩码、I2C 地址、数据结构、枚举类型集中于此文件。修改系统行为只需编辑此文件。详见上方 [配置参数](#配置参数) 表格。

#### 延时函数 (delay.c)

- **`Delay_us(uint16_t us)`**：占用 SysTick 外设做轮询延时。**关键修复**：原实现直接写 `SysTick->CTRL = 0x05` 会永久清除 TICKINT 位导致 1ms 时基中断失效。修复后保存 CTRL + LOAD → 执行延时 → 恢复 CTRL + LOAD。延时精度受 save/restore 开销影响约 ±1us。
- **`Delay_ms(uint16_t ms)`**：循环调用 `Delay_us(1000)`
- **`Delay_s(uint16_t s)`**：循环调用 `Delay_ms(1000)`

#### 时基 (systick.c)

- `SysTick_Init()`：LOAD = 71999 (72MHz 下 72000 周期 = 1ms)，CTRL = CLKSOURCE | TICKINT | ENABLE
- `SysTick_Handler()`：每 1ms 递增 `g_sysTick`。`g_sample_counter` 累加到 1000 时置 `g_sample_flag = 1`。ISR 只置 1，主循环只清 0，构成简化信号量。
