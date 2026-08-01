# STM32F103C8T6 FreeRTOS Joystick OLED 项目

基于 **STM32F103C8T6 (Blue Pill)** 的嵌入式 RTOS 项目。
使用 **STM32CubeMX + VSCode + CMake + GCC ARM** 工具链开发，集成 FreeRTOS 实时内核，
通过 I2C 驱动 SSD1306 OLED 显示，ADC 采样双轴摇杆实现光标控制，
矩阵键盘完成密码登录与桌面操作，SPI 外挂 W25Q64 提供非易失存储。

---

## 硬件资源

| 外设 | 引脚 | 配置 |
|------|------|------|
| **I2C1** (OLED) | PB6 (SCL), PB7 (SDA) | 400kHz Fast Mode |
| **ADC1_IN0** (摇杆 X) | PA0 | 12-bit, 连续扫描 |
| **ADC1_IN1** (摇杆 Y) | PA1 | 12-bit, 连续扫描 |
| **SW** (摇杆按键) | PA2 | GPIO Input, 上拉 |
| **SPI1** (W25Q64) | PA5 (SCK), PA6 (MISO), PA7 (MOSI), PB0 (CS) | 18MHz, SPI Mode 3, CS 低有效空闲高 |
| LED | PB1 | GPIO Output, 推挽 |
| HSE 晶振 | PD0, PD1 | 8MHz -> PLL 72MHz |
| SWD 调试 | PA13 (SWDIO), PA14 (SWCLK) | Serial Wire |

---

## 软件架构

```
┌────────────────────────────────────────────────┐
│            FreeRTOS (CMSIS_V2)                 │
│                                                 │
│  Sw_Adc_Task ──cursor(6B×1)──┐                  │
│  MKey_Task   ──Key(1B×1)─────┼──→ Oled_Task    │
│                                (登录→桌面→应用)   │
│                                                 │
│     BSP 层            Dev 层      App 层        │
│  adc / i2c /       oled / mkey /  *_Sys 任务逻辑 │
│  keypad / w25q64   sflash                      │
│                                                 │
│            STM32 HAL 库                         │
└────────────────────────────────────────────────┘
```

### 任务说明

| 任务 | 函数 | 优先级 | 周期 | 功能 |
|------|------|--------|------|------|
| **Sw_Adc** | `Sw_Adc_Task` | Low1 (9) | 20ms | 摇杆 ADC、按键检测、发 cursor 队列 |
| **MKey** | `MKey_Task` | Low1 (9) | 10ms | 矩阵键盘扫描、消抖、发 Key 队列 |
| **oled** | `Oled_Task` | Low (8) | 10ms | PIN 登录、桌面、应用切换与显示 |
| **defaultTask** | `StartDefaultTask` | Normal (24) | - | 保留备用 |

### 队列通信

| 队列名 | 数据大小 | 深度 | 结构 |
|--------|----------|------|------|
| `cursor` | 6 字节 | 1 | `{cursor_x(i16), cursor_y(i16), button(u8)}` |
| `Key` | 1 字节 | 1 | 按键字符 `0-9 A-D * #` |

---

## 项目结构

```
├── Core/
│   ├── Bsp/
│   │   ├── Inc/              # BSP 头文件
│   │   │   ├── i2c.h         # I2C 驱动 (400kHz)
│   │   │   ├── adc.h         # ADC 双通道驱动
│   │   │   ├── keypad.h      # 4x4 矩阵键盘扫描
│   │   │   └── w25q64.h      # W25Q64 SPI NOR Flash
│   │   └── Src/
│   │       ├── i2c.c         # I2C 读写封装, 自动重试
│   │       ├── adc.c         # ADC 连续扫描读取
│   │       ├── keypad.c      # 矩阵扫描 (PB12-15 行, PA8-11 列)
│   │       └── w25q64.c      # SPI 指令级读写/擦除
│   ├── Dev/
│   │   ├── Inc/
│   │   │   ├── oled.h        # SSD1306 OLED 128x64 API
│   │   │   ├── mkey.h        # 矩阵键盘设备层（消抖/字符映射）
│   │   │   └── sflash.h      # 串行 Flash 扇区级 API
│   │   └── Src/
│   │       ├── oled.c        # OLED 帧缓冲 + 5x7 字库
│   │       ├── mkey.c        # 按键字符 + 边沿事件
│   │       └── sflash.c      # 扇区保存/读取/擦除
│   ├── App/
│   │   ├── Inc/              # 任务层头文件
│   │   │   ├── Oled_Sys.h    # OLED 任务（登录/桌面）
│   │   │   ├── MKey_Sys.h    # 矩阵键盘任务
│   │   │   ├── Sw_Adc_Sys.h  # 摇杆任务 + 共享状态
│   │   │   └── Desktop_Sys.h # 桌面主界面
│   │   └── Src/
│   │       ├── Oled_Sys.c    # 登录 + 桌面入口
│   │       ├── MKey_Sys.c    # 键盘事件入队
│   │       ├── Sw_Adc_Sys.c  # 摇杆事件入队
│   │       └── Desktop_Sys.c # 桌面/应用切换
│   ├── Inc/                  # CubeMX 生成头文件
│   │   ├── main.h            # 含 SW_Pin / SW_GPIO_Port 定义
│   │   ├── FreeRTOSConfig.h
│   │   └── stm32f1xx_hal_conf.h
│   └── Src/                  # CubeMX 生成+用户代码
│       ├── main.c            # 入口 + 任务创建
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

### 矩阵键盘 (`keypad.h`)

4x4 矩阵，行 PB12-PB15、列 PA8-PA11，行线输出逐行拉低扫描。

| 函数 | 说明 |
|------|------|
| `BSP_Keypad_Init` | 行线复位为高电平空闲 |
| `BSP_Keypad_Scan` | 逐行扫描，返回键码 0-15 或 NO_KEY |

### W25Q64 (`w25q64.h`)

SPI NOR Flash，8MB，扇区 4KB、页 256B。`BSP_W25Q64_Write` 前必须先擦除目标区域。

| 函数 | 说明 |
|------|------|
| `BSP_W25Q64_Init` | 读取 JEDEC ID 并校验厂商 |
| `BSP_W25Q64_ReadID` | 读取 3 字节 ID（厂商/类型/容量） |
| `BSP_W25Q64_Read` | 按字节地址读取 |
| `BSP_W25Q64_Write` | 页编程（自动跨页拆分，需先擦除） |
| `BSP_W25Q64_EraseSector` | 4KB 扇区擦除 |
| `BSP_W25Q64_EraseChip` | 整片擦除（耗时可达数十秒） |

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

### 矩阵键盘设备 (`mkey.h`)

在 BSP 扫描之上提供消抖和字符映射。

| 函数 | 说明 |
|------|------|
| `MKey_KeyToChar` | 键码 → 字符 `0-9 A-D * #` |
| `MKey_GetKey` | 20ms 消抖后的当前按键 |
| `MKey_GetKeyEvent` | 边沿事件，每次按下只返回一次 |

### 串行 Flash (`sflash.h`)

基于 W25Q64 BSP 的扇区级设备接口，按 4KB 扇区编号存取。

| 函数 | 说明 |
|------|------|
| `SFlash_Init` | 初始化并校验 Flash |
| `SFlash_EraseSector` | 按扇区编号擦除 |
| `SFlash_SaveSector` | 先擦除再写入整扇区数据（≤4KB） |
| `SFlash_LoadSector` | 读取整扇区数据 |
| `SFlash_Read / Write` | 按字节地址读写（Write 需先擦除） |
| `SFlash_EraseAll` | 整片擦除 |

---

## W25Q64 存储分区

W25Q64 共 **8MB（2048 个 4KB 扇区）**，按功能划分如下：

| 区域 | 扇区范围 | 扇区数 | 容量 | 说明 |
|------|---------|--------|------|------|
| 系统设置 | 0 - 7 | 8 | 32KB | 设置项 + 版本/CRC，多备份防掉电损坏 |
| 文件表 | 8 - 15 | 8 | 32KB | 文件名/类型/大小/位置/状态记录 |
| 文件数据 | 16 - 287 | 272 | 1.06MB | 用户文件内容 |
| 画图数据 | 288 - 543 | 256 | 1MB | 保存的绘图（128x64 每帧 1024B） |
| 音乐数据 | 544 - 799 | 256 | 1MB | 旋律/音效采样 |
| 系统日志 | 800 - 1023 | 224 | 896KB | 环形缓冲，覆盖写入 |
| 预留 | 1024 - 2047 | 1024 | 4MB | 未来 OTA/字体/资源扩展 |

> 关键数据（系统设置、文件表）写入时建议附加版本号和 CRC，异常复位后校验失败即视为无效；日志使用环形缓冲分散到多个扇区，降低同一扇区的擦写频率，延长寿命。

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
| SPI1 | Full Duplex Master, 18MHz, Mode 3, NSS Software |
| FreeRTOS | CMSIS_V2, 4 个任务, 2 个队列 |
| NVIC 时基 | TIM4, 优先级 15 |
| 工具链 | CMake + GCC, -O6 |

---

## FreeRTOS 配置

| 参数 | 值 |
|------|-----|
| 内核 | CMSIS RTOS V2 |
| 任务数 | 4 |
| 队列 | 2 (cursor 6B×1, Key 1B×1) |
| 堆大小 | 3072 bytes |
| 栈大小 | 512 bytes/任务 |

## 依赖

- STM32Cube FW_F1 V1.8.7
- arm-none-eabi-gcc
- CMake ≥ 3.22
- Ninja
- ST-Link (烧录)
