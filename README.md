# STM32F103C8T6 FreeRTOS 多功能桌面系统

基于 **STM32F103C8T6（Blue Pill）** 和 **FreeRTOS（CMSIS-RTOS2）** 的多功能桌面项目。通过 OLED、矩阵键盘、双轴摇杆、W25Q64 外部 Flash、无源蜂鸣器和 DS3231 RTC，实现桌面、绘图、文件管理、音乐播放、系统监控、设置、日志和实时时钟显示。

## 主要功能

- PIN 密码登录，密码保存在 W25Q64
- 桌面 3×2 应用网格，摇杆和键盘都能移动光标
- Draw 绘图：新建、绘制、保存、重命名、删除图片，支持画点/画线/区域擦除三种画笔模式
- File：管理 MUS1 音乐和 DRW1 图片
- Music：后台播放、歌曲页时间显示、暂停/继续、播完自动暂停
- 无源蜂鸣器：TIM3_CH4 PWM 输出不同频率
- Monitor：系统运行时间、输入事件、丢弃事件、错误、HEAP、任务栈水位、设备连接状态
- SET：光标大小、灵敏度、亮度、音量、息屏时长、密码、RTC 时间
- LOG：系统日志写入 W25Q64 环形区，支持实时翻页查看全部日志
- 输入检测：摇杆断开自动识别，桌面左上角显示 `IN YES/NO`
- 错误提示：桌面顶部显示 `ERR n`，新错误出现时闪烁提示
- 息屏：超时自动息屏，按 A 可手动强制息屏；无音乐息屏进入深睡眠低功耗
- DS3231：桌面左下角显示年月日时分秒

## 硬件资源

| 外设 | 引脚 | 说明 |
|------|------|------|
| OLED | PB6/SCL, PB7/SDA | I2C1, SSD1306 128×64 |
| 摇杆 X/Y | PA0/PA1 | ADC1 双通道（DMA1_Channel1 读取） |
| 摇杆按键 | PA2 | GPIO 输入，上拉 |
| W25Q64 | PA5/SCK, PA6/MISO, PA7/MOSI, PB0/CS | SPI1, 8MB |
| 无源蜂鸣器 | PB1 | TIM3_CH4 PWM |
| DS3231 | PB10/SCL, PB11/SDA | I2C2, 100kHz |
| 矩阵键盘 | PB12-PB15 行, PA8-PA11 列 | 4×4 |
| HSE 晶振 | PD0, PD1 | 8MHz → PLL 72MHz |

## 软件架构

```text
FreeRTOS (CMSIS-RTOS2)
  ├─ defaultTask   空转占位任务
  ├─ Oled_Task     登录、桌面、前台应用（含音乐/文件列表界面）
  ├─ MKey_Task     矩阵键盘扫描/消抖/入队
  ├─ Sw_Adc_Task   摇杆 DMA 采样/光标队列
  ├─ Log_Task      日志队列 → W25Q64
  ├─ MusicPlay_Task 后台音乐播放
  ├─ App_File_Task  File 应用
  ├─ Watchdog_Task  心跳检查/喂狗（IWDG）
  └─ Screen_Sleep  息屏 tickless 睡眠（覆盖 FreeRTOS 端口函数）
```

FreeRTOS 还会自动创建 `IDLE` 和 `Tmr Svc` 任务。

## 摇杆 ADC（DMA 读取）

- ADC1 双通道：PA0 = CH0，PA1 = CH1
- 使用 DMA1_Channel1，普通模式，一次传输 2 个结果
- `BSP_ADC_ReadDual()` 固定返回 `buf[0] = CH0`、`buf[1] = CH1`
- 不再依赖轮询时序，上电后左右/上下不会随机互换

## 输入设备连接检测

- 摇杆断开检测：X/Y 各取最近 100 次采样平均值，两轴平均值持续偏向左下角（相对校准中心偏移超过 500）判定断开
- 桌面左上角显示 `IN YES/NO`，Monitor 设备页显示 `JOY OK/FAIL`
- 断开/恢复各写一次日志：`JOY OFF` / `JOY ON`

## 息屏与低功耗

息屏有两种入口：

- 超时自动息屏：`MKey` 任务周期调用 `Screen_Sys_Update()`
- `A` 键强制息屏（登录、命名、密码输入界面除外）

息屏后：

- OLED 发送 `DISPLAY OFF`
- 任意按键或摇杆活动通过 `Screen_Sys_Wake()` 唤醒并恢复任务

### 有音乐息屏

- 挂起 `oled`、`Log` 任务，`MKey` 10ms→50ms，`Sw_Adc` 20ms→200ms
- 音乐照常播放，不进入睡眠

### 无音乐息屏（深睡眠）

- 除 `MKey`、`Sw_Adc` 外全部任务挂起，两者都改为 200ms 轮询
- 两者阻塞后，空闲任务通过覆盖 `vPortSuppressTicksAndSleep()` 进入 `WFI` Sleep（tickless idle）
- 睡眠前 `HAL_SuspendTick()` 停掉 TIM4，醒来后按实际睡眠 tick 补偿 `uwTick` 再 `HAL_ResumeTick()`
- 唤醒延迟上限 200ms；SysTick 24 位计数限制单次最长睡眠约 233ms

> 注意：CMSIS-RTOS2 的 `SysTick_Handler` 会读 `SysTick->CTRL` 清 COUNTFLAG，tickless 记账必须在放行中断前完成，否则每次睡眠只补 1 个 tick，任务无法准时唤醒。

## 队列

| 队列 | 深度 | 消息 |
|------|------|------|
| `cursorHandle` | 4 | 光标坐标 + 摇杆按钮 |
| `KeyHandle` | 1 | 按键字符 |
| `LogQueueHandle` | 8 | 日志类型 + 文本 |

## W25Q64 分区

| 扇区 | 用途 |
|------|------|
| 0 | SET 设置（`SET1`） |
| 1 | 任务异常记录（`ERR1`） |
| 2-287 | 预留 |
| 288-543 | 画图数据（`DRW1`） |
| 544-799 | 音乐数据（`MUS1`） |
| 800 | 日志头 |
| 801-1023 | 日志环形区 |
| 1024 | Music 重命名暂存 |
| 1025-2047 | 预留 |

## 应用说明

### Draw

- `2/8/4/6` 移动画笔
- `C` 切换画笔模式：画点 / 画线 / 区域擦除（光标形状随之变化）
- `5` 画点模式下切换像素；画线/擦除模式下第一次按键定起点，第二次按键确认
- `0` 清空画布
- `#` 保存并命名
- `D` 在列表删除图片

### File

- `0` 重命名
- `D` 删除
- `#` 进入/查看
- 图片列表底部 `NEW`：新建空白图片并进入画图编辑

### Music

- 列表 `#` 进入歌曲页，歌曲默认暂停
- 歌曲页 `1` 开始/暂停
- `*` 返回
- 歌曲页显示已播放秒数/总秒数

### Monitor

- 显示系统运行时间、输入事件、丢弃事件、错误、HEAP、任务栈水位
- 任务列表按任务名排序显示，位置固定
- 最后一页为设备页：`JOY OK/FAIL`、`IN YES/NO`
- `ERR` 计数来源：SET 加载/保存失败、日志清空失败、任务卡死（`HANG`）
- `4/6` 翻页

### SET

- 第 1 页：光标大小、灵敏度、亮度、音量、息屏时长
- 第 2 页：密码修改、RTC 时间设置
- `4/6` 调整，`#` 进入/确认，`*` 返回

### LOG

- 显示日志，翻页时实时从 Flash 读取，不再限制 24 条（最多 4758 页）
- 日志类型：`B` 开机、`S` 设置、`M` 音乐、`A` 应用、`E` 错误、`D` 图片
- `0` 清空日志（带确认）：逐个擦除日志扇区，若清空过程中 Flash 操作失败，会写入 `LOG CLEAR ERR`，并使桌面/Monitor 的 `ERR` 计数加 1
- `4/6` 翻页
- `0` 清空日志（带确认）

## 常用按键

| 按键 | 功能 |
|------|------|
| `2/8/4/6` | 移动/选择/翻页（按界面而定） |
| `#` | 确认/进入 |
| `*` | 返回 |
| `1` | 音乐暂停/继续 |
| `A` | 强制息屏（命名/密码/登录页除外） |
| `C` | Draw 画笔模式切换（画点/画线/区域擦除） |
| `D` | 删除 |
| `0` | File 重命名 / Draw 清空 / Log 清空 |

## 构建与烧录

```powershell
cmake --build build\Debug -j 4
```

`CMakeLists.txt` 中 FreeRTOS 中间件单独使用 `-Os` 编译，应用代码保持 `-O0`。

固件输出：

```text
build/Debug/test1.elf
```

当前编译占用：

```text
RAM:   18336 B / 20 KB   (89.53%)
FLASH: 65284 B / 64 KB   (99.62%)
```

> FLASH 仅剩约 252 字节；继续加功能前建议先把应用代码切到 `-Og`/`-Os`。
