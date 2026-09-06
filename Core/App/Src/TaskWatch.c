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
/* 周期型任务正常只会短阻塞；事件型任务允许长时间阻塞等待输入/标志 */
static const uint8_t taskwatch_periodic[TASKWATCH_COUNT] = {
    1U, 1U, 1U, 0U, 0U, 0U
};
static volatile uint32_t taskwatch_beat[TASKWATCH_COUNT];
static uint8_t taskwatch_hang[TASKWATCH_COUNT];
static uint32_t taskwatch_last_check = 0U;

void TaskWatch_Beat(uint8_t id)
{
    if (id >= TASKWATCH_COUNT) return;

    taskwatch_beat[id] = HAL_GetTick();
    taskwatch_hang[id] = 0U;
}

void TaskWatch_Check(void)
{
    uint32_t now = HAL_GetTick();
    char text[16];

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
        if (taskwatch_beat[i] == 0U) continue;

        /* 挂起是正常状态（息屏/应用切换），不参与超时判断 */
        st = eTaskGetState((TaskHandle_t)taskwatch_handles[i]);
        if (st == eSuspended)
        {
            taskwatch_hang[i] = 0U;
            continue;
        }

        if ((now - taskwatch_beat[i]) < TASKWATCH_TIMEOUT_MS)
        {
            taskwatch_hang[i] = 0U;
            continue;
        }

        /* 事件型任务阻塞等待输入/标志是正常状态，只有空转/饿死才算异常 */
        if (taskwatch_periodic[i] == 0U && st == eBlocked)
        {
            taskwatch_hang[i] = 0U;
            continue;
        }

        if (taskwatch_hang[i] != 0U) continue;

        taskwatch_hang[i] = 1U;
        memcpy(text, "HANG ", 5U);
        strncpy(text + 5U, taskwatch_names[i], sizeof(text) - 1U - 5U);
        text[sizeof(text) - 1U] = '\0';
        Log_Write(LOG_TYPE_ERROR, text);
        Monitor_Sys_ReportError();
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
