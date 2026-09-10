/**
  ******************************************************************************
  * @file           : TaskWatch.c
  * @brief          : Task heartbeat and hang detection
  ******************************************************************************
  */

#include "TaskWatch.h"
#include "Log_App.h"
#include "Monitor_App.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <string.h>

#define TASKWATCH_TIMEOUT_MS      5000U
#define TASKWATCH_CHECK_PERIOD_MS 1000U

extern osThreadId_t MKeyHandle;
extern osThreadId_t Sw_AdcHandle;
extern osThreadId_t LogHandle;
extern osThreadId_t oledHandle;
extern osThreadId_t App_FileHandle;
extern osThreadId_t MusicPlayHandle;

static osThreadId_t taskwatch_handles[TASKWATCH_COUNT] = {
    NULL, NULL, NULL, NULL, NULL, NULL
};
static const char taskwatch_names[TASKWATCH_COUNT][7] = {
    "MKey", "SwAdc", "Log", "Oled", "File", "Music"
};
static volatile uint32_t taskwatch_beat[TASKWATCH_COUNT];
static uint8_t taskwatch_hang[TASKWATCH_COUNT];
/* 1: must produce heartbeat; 0: task is in a legal wait and is not checked */
static volatile uint8_t taskwatch_enabled[TASKWATCH_COUNT] = {
    1U, 1U, 1U, 1U, 1U, 1U
};
static volatile uint8_t taskwatch_debug_hang[TASKWATCH_COUNT];
static uint32_t taskwatch_last_check = 0U;

static void TaskWatch_ReportHang(uint8_t id)
{
    char text[16];

    if (id >= TASKWATCH_COUNT || taskwatch_hang[id] != 0U) return;

    taskwatch_hang[id] = 1U;
    memcpy(text, "HANG ", 5U);
    strncpy(text + 5U, taskwatch_names[id], sizeof(text) - 1U - 5U);
    text[sizeof(text) - 1U] = '\0';
    Log_Write(LOG_TYPE_ERROR, text);
    Log_FlushNow();
    Monitor_Sys_ReportError();
}

void TaskWatch_Beat(uint8_t id)
{
    if (id >= TASKWATCH_COUNT) return;

    if (taskwatch_debug_hang[id] != 0U)
    {
        for (;;)
        {
            __NOP();
        }
    }

    taskwatch_beat[id] = HAL_GetTick();
    taskwatch_hang[id] = 0U;
    taskwatch_enabled[id] = 1U;
}

void TaskWatch_Refresh(uint8_t id)
{
    if (id >= TASKWATCH_COUNT) return;

    taskwatch_beat[id] = HAL_GetTick();
    taskwatch_hang[id] = 0U;
}

void TaskWatch_WaitBegin(uint8_t id)
{
    if (id >= TASKWATCH_COUNT) return;

    taskwatch_enabled[id] = 0U;
    taskwatch_hang[id] = 0U;
}

void TaskWatch_WaitEnd(uint8_t id)
{
    if (id >= TASKWATCH_COUNT) return;

    taskwatch_beat[id] = HAL_GetTick();
    taskwatch_hang[id] = 0U;
    taskwatch_enabled[id] = 1U;
}

uint8_t TaskWatch_CurrentUiId(void)
{
    eTaskState st;

    if (App_FileHandle == NULL) return TASKWATCH_OLED;

    st = eTaskGetState((TaskHandle_t)App_FileHandle);
    if (st != eSuspended && st != eDeleted) return TASKWATCH_FILE;
    return TASKWATCH_OLED;
}

void TaskWatch_BeatCurrentUi(void)
{
    TaskWatch_Beat(TaskWatch_CurrentUiId());
}

void TaskWatch_DebugRequestCurrentUi(void)
{
    uint8_t id = TaskWatch_CurrentUiId();

    taskwatch_enabled[id] = 1U;
    taskwatch_debug_hang[id] = 1U;
}

void TaskWatch_Check(void)
{
    uint32_t now = HAL_GetTick();

    taskENTER_CRITICAL();
    if ((now - taskwatch_last_check) < TASKWATCH_CHECK_PERIOD_MS)
    {
        taskEXIT_CRITICAL();
        return;
    }
    taskwatch_last_check = now;
    taskEXIT_CRITICAL();

    if (taskwatch_handles[0] == NULL)
    {
        taskwatch_handles[TASKWATCH_MKEY] = MKeyHandle;
        taskwatch_handles[TASKWATCH_ADC] = Sw_AdcHandle;
        taskwatch_handles[TASKWATCH_LOG] = LogHandle;
        taskwatch_handles[TASKWATCH_OLED] = oledHandle;
        taskwatch_handles[TASKWATCH_FILE] = App_FileHandle;
        taskwatch_handles[TASKWATCH_MUSIC] = MusicPlayHandle;
    }

    for (uint8_t i = 0U; i < TASKWATCH_COUNT; i++)
    {
        eTaskState st;

        if (taskwatch_handles[i] == NULL) continue;

        if (taskwatch_debug_hang[i] != 0U)
        {
            TaskWatch_ReportHang(i);
            continue;
        }

        if (taskwatch_beat[i] == 0U) continue;

        st = eTaskGetState((TaskHandle_t)taskwatch_handles[i]);
        if (st == eSuspended || st == eDeleted)
        {
            taskwatch_hang[i] = 0U;
            continue;
        }

        if (taskwatch_enabled[i] == 0U)
        {
            taskwatch_hang[i] = 0U;
            continue;
        }

        if ((now - taskwatch_beat[i]) < TASKWATCH_TIMEOUT_MS)
        {
            taskwatch_hang[i] = 0U;
            continue;
        }

        TaskWatch_ReportHang(i);
    }
}

uint8_t TaskWatch_IsOk(uint8_t id)
{
    if (id >= TASKWATCH_COUNT) return 1U;
    return (taskwatch_hang[id] != 0U) ? 0U : 1U;
}

uint8_t TaskWatch_AnyHang(void)
{
    for (uint8_t i = 0U; i < TASKWATCH_COUNT; i++)
    {
        if (taskwatch_hang[i] != 0U) return 1U;
    }
    return 0U;
}
