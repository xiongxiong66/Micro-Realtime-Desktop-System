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
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "oled.h"
#include "main.h"

extern osThreadId_t oledHandle;
extern osThreadId_t LogHandle;
extern osThreadId_t defaultTaskHandle;
extern osThreadId_t App_FileHandle;

#define SCREEN_SUSP_DEFAULT  0x01U
#define SCREEN_SUSP_OLED     0x02U
#define SCREEN_SUSP_LOG      0x04U
#define SCREEN_SUSP_FILE     0x08U
#define SCREEN_SUSP_MUSIC    0x10U

static uint32_t screen_last_activity = 0U;
static uint8_t screen_off = 0U;
static uint8_t force_off_enabled = 1U;
static uint8_t screen_suspended_mask = 0U;

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
        /* 无音乐息屏：除按键/摇杆外全部挂起 */
        Screen_Sys_SuspendOne(defaultTaskHandle, SCREEN_SUSP_DEFAULT);
        Screen_Sys_SuspendOne(App_FileHandle, SCREEN_SUSP_FILE);
        Screen_Sys_SuspendOne(MusicPlayHandle, SCREEN_SUSP_MUSIC);
    }
    else
    {
        /* 音乐播放中：保持原行为，恢复深睡眠时额外挂起的任务 */
        Screen_Sys_ResumeOne(defaultTaskHandle, SCREEN_SUSP_DEFAULT);
        Screen_Sys_ResumeOne(App_FileHandle, SCREEN_SUSP_FILE);
        Screen_Sys_ResumeOne(MusicPlayHandle, SCREEN_SUSP_MUSIC);
    }
}

static void Screen_Sys_ResumeTasks(void)
{
    Screen_Sys_ResumeOne(defaultTaskHandle, SCREEN_SUSP_DEFAULT);
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
    OLED_On();
}

void Screen_Sys_Wake(void)
{
    screen_last_activity = HAL_GetTick();

    if (screen_off)
    {
        OLED_On();
        screen_off = 0U;
        Screen_Sys_ResumeTasks();
        Log_Write(LOG_TYPE_APP, "WAKE");
    }
}

void Screen_Sys_Update(void)
{
    uint32_t timeout;

    if (screen_off)
    {
        /* 息屏期间音乐可能停止，重新评估是否需要深睡眠挂起 */
        Screen_Sys_SuspendTasks();
        return;
    }

    timeout = Set_Sys_GetScreenTimeoutMs();
    if (timeout > 0U && (HAL_GetTick() - screen_last_activity) >= timeout)
    {
        OLED_Off();
        screen_off = 1U;
        Log_Write(LOG_TYPE_APP, "SLEEP");
        Screen_Sys_SuspendTasks();
    }
}

void Screen_Sys_ForceOff(void)
{
    screen_last_activity = HAL_GetTick();
    if (screen_off) return;

    OLED_Off();
    screen_off = 1U;
    Log_Write(LOG_TYPE_APP, "SLEEP");
    Screen_Sys_SuspendTasks();
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
