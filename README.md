# STM32F103C8T6 FreeRTOS 多功能桌面系统

基于 **STM32F103C8T6（Blue Pill）** 和 **FreeRTOS（CMSIS-RTOS2）** 的多功能桌面项目。通过 OLED、矩阵键盘、双轴摇杆、W25Q64 外部 Flash、无源蜂鸣器和 DS3231 RTC，实现桌面、绘图、文件管理、音乐播放、系统监控、设置、日志和实时时钟显示。

## 主要功能

- PIN 密码登录，密码保存在 W25Q64
- 桌面 3×2 应用网格，摇杆和键盘都能移动光标
- Draw 绘图：新建、绘制、保存、重命名、删除图片，支持画点/画线/画空心矩形/空心圆/实心圆/整图反色/矩形擦除
- File：管理 MUS1 音乐和 DRW1 图片
- Music：音乐列表/后台播放/暂停继续/演奏模式（1-7 对应 DO-RE-MI-FA-SOL-LA-SI）
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

## 任务与调度

系统使用 FreeRTOS + CMSIS-RTOS2，抢占式调度，系统节拍 `1000Hz`，即 `1ms` 一个 tick。FreeRTOS 堆大小为 `7000B`，并开启栈溢出检测、内存分配失败钩子、tickless idle。

| 任务 | 栈大小 | CMSIS 优先级 | 主要运行方式 | 职责 |
|------|--------|--------------|--------------|------|
| `Watchdog` | 512B | `osPriorityAboveNormal` | 200ms 周期 | 检查任务心跳、刷新 IWDG；深睡眠也不挂起 |
| `defaultTask` | 512B | `osPriorityNormal` | 1ms 空转占位 | CubeMX 默认占位任务 |
| `MKey` | 512B | `osPriorityLow1` | 10ms / 50ms / 200ms | 矩阵键盘消抖扫描、息屏判断、按键入队、唤醒屏幕 |
| `Sw_Adc` | 512B | `osPriorityLow1` | 20ms / 200ms | 摇杆 ADC 采样、光标队列、输入设备检测、唤醒屏幕 |
| `Log` | 512B | `osPriorityLow1` | 队列 100ms 轮询，1s 刷盘，10s 保存头部 | 日志队列 → W25Q64 环形区 |
| `Oled` | 768B | `osPriorityLow` | 登录/桌面/前台应用循环 | 启动页、PIN 登录、桌面、Draw/Music/Monitor/SET/LOG 前台界面 |
| `App_File` | 512B | `osPriorityLow` | 挂起，进入 File 后恢复 | File 菜单、音乐/图片列表、播放与文件管理 |
| `MusicPlay` | 512B | `osPriorityLow` | 线程标志事件驱动 | 后台歌曲播放、演奏模式音符发声 |

`MKey / Sw_Adc / Log` 属于周期型任务，正常只短阻塞；`Oled / App_File / MusicPlay` 属于事件型任务，允许长时间阻塞等待按键、队列或线程标志。

调度设计要点：

- 抢占式调度，高优先级任务可以打断低优先级任务；任务通过 `osDelay()`、消息队列、线程标志主动让出 CPU。
- `MKey / Sw_Adc / Log` 高于 `Oled`，保证按键、光标和日志不会因界面阻塞而丢失。
- `Watchdog` 使用最高普通任务优先级，普通任务死循环或饿死时仍能持续检查/喂狗，必要时停止喂狗触发 IWDG。
- 前台应用运行在 `Oled` 任务里；进入 File 时桌面会恢复 `App_File` 并挂起 `Oled`，退出 File 后反向恢复，避免两个 UI 任务同时操作 OLED。
- `MusicPlay` 独立于界面任务，歌曲切页/播放/暂停通过 `MUSIC_BG_FLAG_UPDATE` 线程标志通知；进入歌曲页前先调用 `Music_Bg_StopAll()` 清空旧播放状态。

FreeRTOS 还会自动创建 `IDLE` 和 `Tmr Svc` 任务。

### 共享资源保护

| 资源 | 保护方式 | 说明 |
|------|----------|------|
| W25Q64 / SPI1 | `sflash_mutex` | 所有读/写/擦除进入同一把 FreeRTOS 互斥锁，500ms 超时 |
| OLED / I2C1 | `i2c1_mutex` | OLED 刷屏、息屏开关等 I2C 操作串行化，避免传输中途被抢占 |
| 队列 | CMSIS-RTOS2 Queue | `KeyHandle` 深度 1，`cursorHandle` 深度 4，`LogQueueHandle` 深度 8 |

### 心跳与看门狗

- 6 个受监控任务在循环开始时调用 `TaskWatch_Beat()`，心跳超时门限为 `3s`。
- `Watchdog` 每 `200ms` 执行一次 `TaskWatch_Check()`，检查节流为每秒一次。
- 事件型任务处于 `eBlocked` 等待时不判异常；息屏主动挂起的任务也不判异常。
- 首次确认 HANG 时先喂狗并写日志；下一轮仍 HANG 则停止喂狗，等待 IWDG 复位。
- IWDG：LSI `40kHz / 64 = 625Hz`，重载 `1250`，超时约 `2s`。
- 栈溢出/内存分配失败由 `TaskErr` 写入 W25Q64 预留扇区后复位，开机转写为 `STK xxx` / `MALLOC FAIL` 日志。

### 桌面 `ERR n` 计数

桌面顶部和 Monitor 汇总页显示的 `ERR n` 由 `Monitor_Sys_ReportError()` 计数，只在运行过程中累加，不会自动减少。新增一次错误时，桌面 `ERR n` 会闪烁约 4 秒，之后保持常亮。

以下情况会使 `ERR n` 加 1：

| 情况 | 触发条件 |
|------|----------|
| 设置加载失败 | 开机读取 W25Q64 设置扇区失败、设置记录无效或无法初始化 Flash，加载默认设置时 |
| 设置保存失败 | 保存设置时 Flash 初始化失败，或擦除/写入/回读校验连续失败 |
| 日志清空失败 | 在 LOG 应用清空日志过程中，任一日志扇区擦除失败 |
| 任务卡死 | `TaskWatch` 首次判定某个监控任务 HANG 并写入 `HANG xxx` 日志时 |

不会增加 `ERR n` 的情况：

- 只写入 `E` 类型日志但未调用 `Monitor_Sys_ReportError()` 的事件，例如 `LOGIN FAIL`、`JOY OFF`、开机 `WDG RESET` 记录。
- 栈溢出/内存分配失败通过 `TaskErr` 复位处理，本身不调用桌面错误计数。
- Draw/Music/File 的单次保存、重命名、删除或读取失败，如果只返回失败但没调用 `Monitor_Sys_ReportError()`，当前也不会使 `ERR n` 增加。

> 注意：`ERR n` 是 RAM 内的运行期指标，掉电或看门狗复位后会清零。卡死最终触发 IWDG 复位时，用于事后定位的依据是 W25Q64 日志中的 `HANG xxx` 和开机 `WDG RESET`，不是桌面上的 `ERR n`。

### 与息屏睡眠的关系

- 亮屏时：`MKey` 10ms、`Sw_Adc` 20ms，界面任务正常刷新。
- 息屏后：OLED 关屏，`Oled`、`Log` 挂起；`MKey` 降为 50ms，`Sw_Adc` 降为 200ms。
- 若音乐正在播放，`MusicPlay` 继续运行，不进入 tickless 睡眠。
- 若息屏且无音乐（深睡眠）：`defaultTask / Oled / Log / App_File / MusicPlay` 全部挂起，只保留 `MKey / Sw_Adc / Watchdog` 运行。
- 深睡眠时 `Watchdog` 不挂起，保持每 200ms 喂狗，避免正常睡眠触发 IWDG 复位。
- 空闲时 `vPortSuppressTicksAndSleep()` 使用 SysTick 设置睡眠时长进入 `WFI`；睡眠前停 HAL 时间基准 TIM4，醒来后补偿 RTOS tick 和 `uwTick`，再恢复 TIM4。
- 按键或摇杆活动会通过 `Screen_Sys_Wake()` 唤醒并恢复所有被挂起任务，写 `WAKE` 日志。

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

- 除 `MKey`、`Sw_Adc`、`Watchdog` 外全部任务挂起，`MKey`/`Sw_Adc` 改为 200ms 轮询
- `Watchdog` 保持运行并继续每 200ms 喂狗，避免睡眠期间被 IWDG 复位
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
- `C` 切换画笔模式：画点 / 画线 / 画空心矩形 / 空心圆 / 实心圆 / 整图反色（光标形状随之变化）
- `5` 画点模式下切换像素；其它绘制模式下第一次按键定起点，第二次按键确认
- `0` 清空画布
- `#` 保存并命名
- `D` 在列表删除图片
- 编辑页按 `D` 直接进入矩形擦除模式
- 整图反色：进入 `INV` 模式后按 `5` 或摇杆按钮执行整张图反色

### File

- `0` 重命名
- `D` 删除
- `#` 进入/查看
- 图片列表底部 `NEW`：先进入命名界面，确认后直接创建空白图片

### Music

- 进入 MUSI 后先显示 `PLAY / SONGS` 菜单
- `PLAY` 进入演奏模式：`1-7` 分别对应 `DO / RE / MI / FA / SOL / LA / SI`
- 演奏模式每按一个键发声约 250ms，屏幕显示当前音符，日志记录 `KEY1 DO` 等
- `SONGS` 进入歌曲列表，`#` 打开歌曲页，歌曲默认暂停
- 歌曲页 `1` 开始/暂停
- `*` 返回
- 歌曲页显示已播放秒数/总秒数
- 进入歌曲页前会先清空旧的后台播放状态，避免播放到上一首内容

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
| `1` | 歌曲页暂停/继续；演奏模式下为 `DO` |
| `A` | 强制息屏（命名/密码/登录页除外） |
| `C` | Draw 画笔模式切换（画点/画线/矩形/圆/反色） |
| `D` | 删除；Draw 编辑页进入矩形擦除 |
| `0` | File 重命名 / Draw 清空 / Log 清空 |

## 构建与烧录

```powershell
cmake --build build\Debug -j 4
```

`CMakeLists.txt` 中 FreeRTOS 中间件和应用代码均使用 `-Os` 编译。

固件输出：

```text
build/Debug/test1.elf
```

当前编译占用：

```text
RAM:   18456 B / 20 KB   (90.12%)
FLASH: 57224 B / 64 KB   (87.32%)
```
