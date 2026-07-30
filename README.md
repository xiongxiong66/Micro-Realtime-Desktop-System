# STM32F103C8T6 FreeRTOS Joystick OLED 项目

基于 **STM32F103C8T6 (Blue Pill)** 的嵌入式 RTOS 项目。
使用 **STM32CubeMX + VSCode + CMake + GCC ARM** 工具链开发，集成 FreeRTOS 实时内核，
通过 I2C 驱动 SSD1306 OLED 显示，ADC 采样双轴摇杆实现光标控制。

---

## 硬件资源

| 外设 | 引脚 | 配置 |
|------|------|------|
| **I2C1** (OLED) | PB6 (SCL), PB7 (SDA) | 400kHz Fast Mode |
| **ADC1_IN0** (摇杆 X) | PA0 | 12-bit, 连续扫描 |
| **ADC1_IN1** (摇杆 Y) | PA1 | 12-bit, 连续扫描 |
| **SW** (摇杆按键) | PA2 | GPIO Input, 上拉 |
| LED | PB0, PB1 | GPIO Output, 推挽 |
| HSE 晶振 | PD0, PD1 | 8MHz -> PLL 72MHz |
| SWD 调试 | PA13 (SWDIO), PA14 (SWCLK) | Serial Wire |

---

## 软件架构

```
┌────────────────────────────────────────────────┐
│            FreeRTOS (CMSIS_V2)                 │
│                                                 │
│  ┌──────────────┐      Queue      ┌──────────┐ │
│  │  Sw_Adc_Task  │──(cursor)────→│ Oled_Task│ │
│  │  (Low1, 20ms) │   6 bytes     │(Low,15ms)│ │
│  └──────┬───────┘               └─────┬────┘ │
│         │                             │      │
│    ┌────┴────┐                   ┌────┴────┐  │
│    │  BSP    │                   │  Dev    │  │
│    │ adc.c/h │                   │ oled.c/h│  │
│    │ i2c.c/h │                   │         │  │
│    └─────────┘                   └─────────┘  │
│         │                             │      │
│    ┌────┴─────────────────────────────┴────┐  │
│    │         STM32 HAL 库                  │  │
│    └───────────────────────────────────────┘  │
└────────────────────────────────────────────────┘
```

### 任务说明

| 任务 | 函数 | 优先级 | 周期 | 功能 |
|------|------|--------|------|------|
| **Sw_Adc** | `Sw_Adc_Task` | Low1 (9) | 20ms | 读取摇杆 ADC、按键检测、发队列 |
| **oled** | `Oled_Task` | Low (8) | 15ms | 收队列、绘制光标、刷新 OLED |
| **defaultTask** | `StartDefaultTask` | Normal (24) | - | 保留备用 |

### 队列通信

| 队列名 | 数据大小 | 深度 | 结构 |
|--------|----------|------|------|
| `cursor` | 6 字节 | 1 | `{cursor_x(u16), cursor_y(u16), button(u8)}` |

---

## 项目结构

```
├── Core/
│   ├── Bsp/
│   │   ├── Inc/              # BSP 头文件
│   │   │   ├── i2c.h         # I2C 驱动 (400kHz)
│   │   │   └── adc.h         # ADC 双通道驱动
│   │   └── Src/
│   │       ├── i2c.c         # I2C 读写封装, 自动重试
│   │       └── adc.c         # ADC 连续扫描读取
│   ├── Dev/
│   │   ├── Inc/
│   │   │   └── oled.h        # SSD1306 OLED 128x64 API
│   │   └── Src/
│   │       └── oled.c        # OLED 帧缓冲 + 5x7 字库
│   ├── Inc/                  # CubeMX 生成头文件
│   │   ├── main.h            # 含 SW_Pin / SW_GPIO_Port 定义
│   │   ├── FreeRTOSConfig.h
│   │   └── stm32f1xx_hal_conf.h
│   └── Src/                  # CubeMX 生成+用户代码
│       ├── main.c            # 入口 + 两个任务实现
│       ├── freertos.c        # FreeRTOS 配置
│       └── ...
├── Drivers/                  # HAL / CMSIS 驱动
├── Middlewares/              # FreeRTOS 源码
├── build/                    # 构建输出
├── cmake/                    # CMake 配置
├── test1.ioc                 # CubeMX 工程文件
├── CMakeLists.txt
├── startup_stm32f103xb.s
└── STM32F103XX_FLASH.ld
```

---

## BSP 层 API

### I2C (`i2c.h`)

围绕 `BSP_I2C_Mem_Write/Read` 封装，dev_addr 使用 **7-bit 左对齐**地址（函数内部左移 1 位）。

| 函数 | 说明 |
|------|------|
| `BSP_I2C1_Init/DeInit` | 初始/反初始化 |
| `BSP_I2C_Master_Transmit/Receive` | 主机收发（3 次自动重试） |
| `BSP_I2C_Mem_Write/Read` | 存储器读写（EEPROM 24Cxx / OLED） |
| `BSP_I2C_IsDeviceReady` | 检测设备在线 |
| `BSP_I2C_Scan` | 扫描总线设备 |

### ADC (`adc.h`)

ADC1 配置为连续扫描模式（CH0 + CH1），`BSP_ADC_ReadDual` 内部丢弃首个值以对齐序列边界。

| 函数 | 说明 |
|------|------|
| `BSP_ADC1_Init/DeInit` | 初始/反初始化 |
| `BSP_ADC_ReadDual` | 读取双通道值（CH0 → IN0, CH1 → IN1） |
| `BSP_ADC_ConvertToMillivolt` | ADC 原始值 → mV |

---

## Dev 层 API

### OLED (`oled.h`)

SSD1306 128x64，I2C 地址 0x3C（7-bit）。使用 1024 字节帧缓冲。

| 函数 | 说明 |
|------|------|
| `OLED_Init` | 初始化序列（电荷泵、对比度、扫描方向等） |
| `OLED_Display` | 帧缓冲 → GDDRAM 刷新 |
| `OLED_Clear / Fill` | 清屏 / 全亮 |
| `OLED_DrawPixel` | 像素操作 |
| `OLED_SetCursor / PrintChar / PrintString` | 文本输出（5x7 字库） |
| `OLED_PrintNum / PrintSignedNum` | 数字输出（10/16 进制） |
| `OLED_On / Off / Invert` | 显示控制 |

---

## 方向映射

| 摇杆操作 | ADC 变化 | 光标行为 |
|----------|----------|----------|
| 往右推 | dx > 0 | cursor_x--（左移，可变步长） |
| 往左推 | dx < 0 | cursor_x++（右移，可变步长） |
| 往上推 | dy > 0 | cursor_y++（下移，可变步长） |
| 往下推 | dy < 0 | cursor_y--（上移，可变步长） |

> 方向取决于摇杆模块的电压极性，可在 `Sw_Adc_Task` 中调整。

---

## 构建与烧录

```bash
# 构建
cd build
cmake .. -G Ninja
ninja

# 烧录 (ST-Link)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/Debug/test1.elf verify reset exit"
```

---

## CubeMX 配置概要

| 项目 | 配置 |
|------|------|
| MCU | STM32F103C8T6, LQFP48 |
| 系统时钟 | HSE 8MHz → PLL x9 → 72MHz |
| ADC1 | Scan=ENABLE, Continuous=ENABLE, NbrOfConv=2 |
| I2C1 | Fast Mode (400kHz) |
| FreeRTOS | CMSIS_V2, 3 个任务, 1 个队列 |
| NVIC 时基 | TIM4, 优先级 15 |
| 工具链 | CMake + GCC, -O6 |

---

## FreeRTOS 配置

| 参数 | 值 |
|------|-----|
| 内核 | CMSIS RTOS V2 |
| 任务数 | 3 |
| 队列 | 1 (cursor, 6 bytes × 1) |
| 堆大小 | 0x200 (512 bytes) |
| 栈大小 | 0x400 (1024 bytes) |

## 依赖

- STM32Cube FW_F1 V1.8.7
- arm-none-eabi-gcc
- CMake ≥ 3.22
- Ninja
- ST-Link (烧录)
