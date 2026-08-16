/**
  ******************************************************************************
  * @file           : Screen_App.c
  * @brief          : Turns the OLED off after inactivity and wakes on input
  ******************************************************************************
  */

#include "Screen_App.h"
#include "Set_App.h"
#include "Log_App.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "oled.h"
#include "main.h"

extern osThreadId_t oledHandle;
extern osThreadId_t LogHandle;

static uint32_t screen_last_activity = 0U;
static uint8_t screen_off = 0U;
static uint8_t force_off_enabled = 1U;
static uint8_t screen_suspended_oled = 0U;

static void Screen_Sys_SuspendTasks(void)
{
    /* oled may already be suspended by the file manager handoff, so only
       suspend it here when it is actually running and remember that we did. */
    if (oledHandle != NULL && eTaskGetState((TaskHandle_t)oledHandle) != eSuspended)
    {
        osThreadSuspend(oledHandle);
        screen_suspended_oled = 1U;
    }

    if (LogHandle != NULL && eTaskGetState((TaskHandle_t)LogHandle) != eSuspended)
    {
        osThreadSuspend(LogHandle);
    }

}

static void Screen_Sys_ResumeTasks(void)
{
    if (screen_suspended_oled != 0U && oledHandle != NULL)
    {
        osThreadResume(oledHandle);
        screen_suspended_oled = 0U;
    }

    if (LogHandle != NULL && eTaskGetState((TaskHandle_t)LogHandle) == eSuspended)
    {
        osThreadResume(LogHandle);
    }

}

void Screen_Sys_Init(void)
{
    screen_last_activity = HAL_GetTick();
    screen_off = 0U;
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

    if (screen_off) return;

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
