/**
  ******************************************************************************
  * @file           : Screen_App.c
  * @brief          : Turns the OLED off after inactivity and wakes on input
  ******************************************************************************
  */

#include "Screen_App.h"
#include "Set_App.h"
#include "Log_App.h"
#include "Music_App.h"
#include "Oled_App.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "oled.h"
#include "main.h"

extern osThreadId_t oledHandle;
extern osThreadId_t LogHandle;
extern osThreadId_t defaultTaskHandle;
extern osThreadId_t App_FileHandle;

#define SCREEN_SUSP_OLED     0x01U
#define SCREEN_SUSP_LOG      0x02U
#define SCREEN_SUSP_FILE     0x04U
#define SCREEN_SUSP_MUSIC    0x08U

#define SCREEN_SLEEP_MODE_NONE  0U
#define SCREEN_SLEEP_MODE_FORCE 1U
#define SCREEN_SLEEP_MODE_AUTO  2U

#define SCREEN_LOCK_FLAG 0x01U

static uint32_t screen_last_activity = 0U;
static uint8_t screen_off = 0U;
static uint8_t force_off_enabled = 1U;
static uint8_t screen_suspended_mask = 0U;
static uint8_t screen_sleep_mode = SCREEN_SLEEP_MODE_NONE;
static uint32_t screen_off_tick = 0U;
static volatile uint8_t screen_lock_active = 0U;
static volatile uint8_t screen_lock_requested = 0U;
static uint8_t screen_snapshot[OLED_BUFFER_SIZE];

uint8_t Screen_Sys_IsDeepOff(void)
{
    return (screen_off != 0U && Music_Bg_IsPlaying() == 0U) ? 1U : 0U;
}

static void Screen_Sys_SuspendOne(osThreadId_t handle, uint8_t bit)
{
    if (handle != NULL && (screen_suspended_mask & bit) == 0U &&
        eTaskGetState((TaskHandle_t)handle) != eSuspended)
    {
        osThreadSuspend(handle);
        screen_suspended_mask |= bit;
    }
}

static void Screen_Sys_ResumeOne(osThreadId_t handle, uint8_t bit)
{
    if (handle != NULL && (screen_suspended_mask & bit) != 0U)
    {
        osThreadResume(handle);
        screen_suspended_mask &= (uint8_t)~bit;
    }
}

static void Screen_Sys_SuspendTasks(void)
{
    /* 息屏时都挂起 OLED 和日志任务 */
    Screen_Sys_SuspendOne(oledHandle, SCREEN_SUSP_OLED);
    Screen_Sys_SuspendOne(LogHandle, SCREEN_SUSP_LOG);

    if (Screen_Sys_IsDeepOff())
    {
        /* 无音乐息屏：除按键/摇杆/锁屏任务外全部挂起 */
        Screen_Sys_SuspendOne(App_FileHandle, SCREEN_SUSP_FILE);
        Screen_Sys_SuspendOne(MusicPlayHandle, SCREEN_SUSP_MUSIC);
    }
    else
    {
        /* 音乐播放中：恢复深睡眠时额外挂起的任务 */
        Screen_Sys_ResumeOne(App_FileHandle, SCREEN_SUSP_FILE);
        Screen_Sys_ResumeOne(MusicPlayHandle, SCREEN_SUSP_MUSIC);
    }
}

static void Screen_Sys_ResumeTasks(void)
{
    Screen_Sys_ResumeOne(oledHandle, SCREEN_SUSP_OLED);
    Screen_Sys_ResumeOne(LogHandle, SCREEN_SUSP_LOG);
    Screen_Sys_ResumeOne(App_FileHandle, SCREEN_SUSP_FILE);
    Screen_Sys_ResumeOne(MusicPlayHandle, SCREEN_SUSP_MUSIC);
}

void Screen_Sys_Init(void)
{
    screen_last_activity = HAL_GetTick();
    screen_off = 0U;
    screen_suspended_mask = 0U;
    screen_sleep_mode = SCREEN_SLEEP_MODE_NONE;
    screen_off_tick = 0U;
    screen_lock_active = 0U;
    screen_lock_requested = 0U;
    OLED_On();
}

void Screen_Sys_Wake(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t lock_needed = 0U;

    screen_last_activity = now;

    if (!screen_off)
    {
        return;
    }

    OLED_On();
    screen_off = 0U;

    if (screen_sleep_mode == SCREEN_SLEEP_MODE_FORCE)
    {
        lock_needed = 1U;
    }
    else if (screen_sleep_mode == SCREEN_SLEEP_MODE_AUTO
          && (now - screen_off_tick) >= Set_Sys_GetLockDelayMs())
    {
        lock_needed = 1U;
    }

    screen_sleep_mode = SCREEN_SLEEP_MODE_NONE;

    if (lock_needed != 0U)
    {
        screen_lock_requested = 1U;
        if (defaultTaskHandle != NULL)
        {
            (void)osThreadFlagsSet(defaultTaskHandle, SCREEN_LOCK_FLAG);
        }
        Log_Write(LOG_TYPE_APP, "WAKE");
        return;
    }

    Screen_Sys_ResumeTasks();
    Log_Write(LOG_TYPE_APP, "WAKE");
}

void Screen_Sys_Update(void)
{
    uint32_t timeout;

    if (Screen_Sys_IsLocking()) return;

    if (screen_off)
    {
        /* 息屏期间音乐可能停止，重新评估是否需要深睡眠挂起 */
        Screen_Sys_SuspendTasks();
        return;
    }

    timeout = Set_Sys_GetScreenTimeoutMs();
    if (timeout > 0U && (HAL_GetTick() - screen_last_activity) >= timeout)
    {
        OLED_Capture(screen_snapshot);
        OLED_Off();
        screen_off = 1U;
        screen_sleep_mode = SCREEN_SLEEP_MODE_AUTO;
        screen_off_tick = HAL_GetTick();
        Log_Write(LOG_TYPE_APP, "SLEEP");
        Screen_Sys_SuspendTasks();
    }
}

void Screen_Sys_ForceOff(void)
{
    screen_last_activity = HAL_GetTick();
    if (screen_off) return;

    OLED_Capture(screen_snapshot);
    OLED_Off();
    screen_off = 1U;
    screen_sleep_mode = SCREEN_SLEEP_MODE_FORCE;
    screen_off_tick = HAL_GetTick();
    Log_Write(LOG_TYPE_APP, "SLEEP");
    Screen_Sys_SuspendTasks();
}

void Screen_Lock_Task_Sys(void)
{
    uint32_t flags;

    for (;;)
    {
        flags = osThreadFlagsWait(SCREEN_LOCK_FLAG, osFlagsWaitAny,
                                  osWaitForever);
        if ((flags & SCREEN_LOCK_FLAG) == 0U) continue;

        screen_lock_active = 1U;
        state = Statue_NO;
        Screen_Sys_SetForceOff(0U);

        Oled_Login_Run();

        Screen_Sys_SetForceOff(1U);
        screen_lock_active = 0U;
        screen_lock_requested = 0U;

        OLED_Blit(screen_snapshot);
        OLED_Display();
        Screen_Sys_ResumeTasks();
    }
}

uint8_t Screen_Sys_IsLocking(void)
{
    return (screen_lock_requested != 0U || screen_lock_active != 0U) ? 1U : 0U;
}

void Screen_Sys_SetForceOff(uint8_t enable)
{
    force_off_enabled = enable ? 1U : 0U;
}

uint8_t Screen_Sys_ForceOffEnabled(void)
{
    return force_off_enabled;
}

uint8_t Screen_Sys_IsOff(void)
{
    return screen_off;
}
