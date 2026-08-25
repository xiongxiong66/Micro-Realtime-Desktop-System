/**
  ******************************************************************************
  * @file           : Monitor_App.c
  * @brief          : System monitor app with task/stack diagnostics
  ******************************************************************************
  */

#include "Monitor_App.h"
#include "InputDev_App.h"
#include "Oled_App.h"
#include "Sw_Adc_App.h"
#include "MKey_App.h"
#include "Music_App.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

#define MONITOR_MAX_TASKS    16U
#define MONITOR_TASKS_PER_PAGE 6U

static TaskStatus_t monitor_task_status[MONITOR_MAX_TASKS];
static volatile uint32_t monitor_error_count = 0U;

static void Monitor_SortByName(UBaseType_t count)
{
    for (UBaseType_t i = 0U; i + 1U < count; i++)
    {
        for (UBaseType_t j = 0U; j + 1U < count - i; j++)
        {
            if (strcmp(monitor_task_status[j].pcTaskName,
                       monitor_task_status[j + 1U].pcTaskName) > 0)
            {
                TaskStatus_t tmp = monitor_task_status[j];
                monitor_task_status[j] = monitor_task_status[j + 1U];
                monitor_task_status[j + 1U] = tmp;
            }
        }
    }
}

void Monitor_Sys_ReportError(void)
{
    monitor_error_count++;
}

static void Monitor_PrintName(const char *name)
{
    uint8_t n = 0U;

    while (n < 8U && name != NULL && name[n] != '\0')
    {
        OLED_PrintChar(name[n]);
        n++;
    }
    while (n < 8U)
    {
        OLED_PrintChar(' ');
        n++;
    }
}

static char Monitor_StateChar(eTaskState st)
{
    switch (st)
    {
        case eRunning:   return 'R';
        case eReady:     return 'r';
        case eBlocked:   return 'B';
        case eSuspended: return 'S';
        case eDeleted:   return 'D';
        default:         return '?';
    }
}

static uint8_t Monitor_GetTaskData(UBaseType_t *task_count)
{
    UBaseType_t count = uxTaskGetNumberOfTasks();

    if (count > MONITOR_MAX_TASKS) count = MONITOR_MAX_TASKS;

    *task_count = count;
    if (count == 0U) return 0U;

    if (uxTaskGetSystemState(monitor_task_status, count, NULL) != count) return 0U;

    Monitor_SortByName(count);
    return 1U;
}

static void Monitor_DrawHeader(uint8_t page, uint8_t pages)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("MON ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);
}

static void Monitor_DrawFooter(void)
{
    OLED_SetCursor(0, 56);
    OLED_PrintString("4/6:PG *:BACK");
}

static void Monitor_DrawSummary(uint8_t page, uint8_t pages, UBaseType_t task_count)
{
    uint32_t sec = HAL_GetTick() / 1000U;
    uint32_t hh = sec / 3600U;
    uint32_t mm = (sec / 60U) % 60U;
    uint32_t ss = sec % 60U;
    uint32_t free_heap = xPortGetFreeHeapSize();

    Monitor_DrawHeader(page, pages);

    OLED_SetCursor(0, 8);
    OLED_PrintString("UP ");
    OLED_PrintNum(hh, 10);
    OLED_PrintChar(':');
    if (mm < 10U) OLED_PrintChar('0');
    OLED_PrintNum(mm, 10);
    OLED_PrintChar(':');
    if (ss < 10U) OLED_PrintChar('0');
    OLED_PrintNum(ss, 10);

    OLED_SetCursor(0, 16);
    OLED_PrintString("SYS ");
    OLED_PrintString((state == Statue_Yes) ? "OK" : "PIN");

    OLED_SetCursor(0, 24);
    OLED_PrintString("EV ");
    OLED_PrintNum(adc_events + mkey_events, 10);
    OLED_PrintString(" DRP ");
    OLED_PrintNum(adc_dropped + mkey_dropped, 10);

    OLED_SetCursor(0, 32);
    OLED_PrintString("ERR ");
    OLED_PrintNum(monitor_error_count, 10);

    OLED_SetCursor(0, 40);
    OLED_PrintString("HEAP ");
    OLED_PrintNum(free_heap, 10);

    OLED_SetCursor(0, 48);
    OLED_PrintString("TASKS ");
    OLED_PrintNum((uint32_t)task_count, 10);

    Monitor_DrawFooter();
    OLED_Display();
}

static void Monitor_DrawTasks(uint8_t page, uint8_t pages, UBaseType_t task_count)
{
    uint8_t start = (uint8_t)((page - 1U) * MONITOR_TASKS_PER_PAGE);

    Monitor_DrawHeader(page, pages);

    for (uint8_t row = 0U; row < MONITOR_TASKS_PER_PAGE; row++)
    {
        uint8_t idx = (uint8_t)(start + row);

        if (idx >= task_count) break;

        OLED_SetCursor(0, (uint8_t)(8U + row * 8U));
        Monitor_PrintName(monitor_task_status[idx].pcTaskName);
        OLED_PrintChar(' ');
        OLED_PrintChar(Monitor_StateChar(monitor_task_status[idx].eCurrentState));
        OLED_PrintChar(' ');
        OLED_PrintNum((uint32_t)monitor_task_status[idx].usStackHighWaterMark
                      * (uint32_t)sizeof(StackType_t), 10);
    }

    Monitor_DrawFooter();
    OLED_Display();
}

static void Monitor_DrawDevices(uint8_t page, uint8_t pages)
{
    Monitor_DrawHeader(page, pages);

    OLED_SetCursor(0, 16);
    OLED_PrintString("JOY ");
    OLED_PrintString(InputDev_IsConnected() ? "OK" : "FAIL");

    OLED_SetCursor(0, 24);
    OLED_PrintString("IN ");
    OLED_PrintString(InputDev_IsConnected() ? "YES" : "NO");

    Monitor_DrawFooter();
    OLED_Display();
}

void Monitor_Sys_Run(void)
{
    uint8_t page = 0U;
    char key;
    uint32_t last_refresh = 0U;

    for (;;)
    {
        uint32_t now = HAL_GetTick();
        UBaseType_t task_count = 0U;
        uint8_t pages;

        if ((now - last_refresh) >= 1000U)
        {
            last_refresh = now;

            (void)Monitor_GetTaskData(&task_count);
            pages = (uint8_t)(1U + (task_count + MONITOR_TASKS_PER_PAGE - 1U) / MONITOR_TASKS_PER_PAGE + 1U);
            if (page >= pages) page = (uint8_t)(pages - 1U);

            if (page == 0U)
            {
                Monitor_DrawSummary(page, pages, task_count);
            }
            else if (page + 1U == pages)
            {
                Monitor_DrawDevices(page, pages);
            }
            else
            {
                Monitor_DrawTasks(page, pages, task_count);
            }
        }

        if (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK)
        {
            if (key == '*') return;
            else if (key == '1') Music_Bg_Toggle();
            else if (key == '4' || key == '6')
            {
                UBaseType_t count = uxTaskGetNumberOfTasks();

                if (count > MONITOR_MAX_TASKS) count = MONITOR_MAX_TASKS;
                pages = (uint8_t)(1U + (count + MONITOR_TASKS_PER_PAGE - 1U) / MONITOR_TASKS_PER_PAGE + 1U);

                if (key == '4')
                {
                    if (page > 0U) page--;
                }
                else if (page + 1U < pages)
                {
                    page++;
                }
                last_refresh = 0U;
            }
        }
        osDelay(10U);
    }
}
