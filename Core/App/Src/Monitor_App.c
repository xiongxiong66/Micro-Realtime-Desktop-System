/**
  ******************************************************************************
  * @file           : Monitor_App.c
  * @brief          : System monitor app with task/stack diagnostics
  ******************************************************************************
  */

#include "Monitor_App.h"
#include "InputDev_App.h"
#include "TaskWatch.h"
#include "Oled_App.h"
#include "Sw_Adc_App.h"
#include "MKey_App.h"
#include "Music_App.h"
#include "Log_App.h"
#include "CursorView.h"
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

uint32_t Monitor_Sys_GetErrorCount(void)
{
    return monitor_error_count;
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

static uint8_t Monitor_TaskPages(UBaseType_t task_count)
{
    uint8_t pages = (uint8_t)((task_count + MONITOR_TASKS_PER_PAGE - 1U)
                              / MONITOR_TASKS_PER_PAGE);
    return (pages == 0U) ? 1U : pages;
}

static uint8_t Monitor_PageCount(UBaseType_t task_count)
{
    /* summary + task pages + queue + devices */
    return (uint8_t)(Monitor_TaskPages(task_count) + 3U);
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

static void Monitor_PresentPage(void)
{
    OLED_Display();
    CursorView_Place();
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

    Monitor_PresentPage();
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

    Monitor_PresentPage();
}

static void Monitor_DrawQueueRow(const char *name, osMessageQueueId_t queue,
                                 uint8_t y)
{
    uint32_t count = (queue != NULL) ? osMessageQueueGetCount(queue) : 0U;
    uint32_t capacity = (queue != NULL) ? osMessageQueueGetCapacity(queue) : 0U;
    uint32_t space = (capacity > count) ? (capacity - count) : 0U;

    OLED_SetCursor(0, y);
    OLED_PrintString(name);
    OLED_PrintNum(count, 10);
    OLED_PrintChar('/');
    OLED_PrintNum(capacity, 10);
    OLED_PrintString(" SP ");
    OLED_PrintNum(space, 10);
}

static void Monitor_DrawQueues(uint8_t page, uint8_t pages)
{
    Monitor_DrawHeader(page, pages);

    Monitor_DrawQueueRow("CUR ", cursorHandle, 8U);
    Monitor_DrawQueueRow("KEY ", KeyHandle, 16U);
    Monitor_DrawQueueRow("LOG ", LogQueueHandle, 24U);

    Monitor_PresentPage();
}

static void Monitor_RefreshQueueRow(const char *name, osMessageQueueId_t queue,
                                    uint8_t y)
{
    CursorView_Erase();

    OLED_FillRect(0, y, OLED_WIDTH, 8U, OLED_BLACK);
    Monitor_DrawQueueRow(name, queue, y);
    OLED_UpdateRect(0, y, OLED_WIDTH, 8U);

    CursorView_Place();
}

static const char *const monitor_beat_names[TASKWATCH_COUNT] = {
    "MKey", "Adc", "Log", "Oled", "File", "Music"
};

static void Monitor_DrawBeat(uint8_t id, uint8_t x, uint8_t y)
{
    OLED_SetCursor(x, y);
    OLED_PrintString(monitor_beat_names[id]);
    OLED_PrintChar(' ');
    OLED_PrintString(TaskWatch_IsOk(id) ? "OK" : "HANG");
}

static void Monitor_DrawDevices(uint8_t page, uint8_t pages)
{
    Monitor_DrawHeader(page, pages);

    OLED_SetCursor(0, 8);
    OLED_PrintString("JOY ");
    OLED_PrintString(InputDev_IsConnected() ? "OK" : "FAIL");

    for (uint8_t row = 0U; row < 3U; row++)
    {
        uint8_t y = (uint8_t)(16U + row * 8U);
        Monitor_DrawBeat(row, 0U, y);
        Monitor_DrawBeat((uint8_t)(row + 3U), 64U, y);
    }

    OLED_SetCursor(0, 40);
    OLED_PrintString("IN ");
    OLED_PrintString(InputDev_IsConnected() ? "YES" : "NO");

    Monitor_PresentPage();
}

void Monitor_Sys_Run(void)
{
    uint8_t page = 0U;
    uint8_t task_pages = 1U;
    uint8_t pages = 4U;
    uint8_t queue_page_ready = 0U;
    uint32_t queue_slow_refresh = 0U;
    char key;
    uint32_t last_refresh = 0U;
    CursorMsg_t drop;

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_OLED);

        uint32_t now = HAL_GetTick();
        UBaseType_t task_count = 0U;
        uint8_t queue_page = (uint8_t)(task_pages + 1U);
        uint32_t refresh_ms = (page == queue_page) ? 25U : 1000U;

        while (osMessageQueueGet(cursorHandle, &drop, NULL, 0U) == osOK) { }

        CursorView_Track();

        if ((now - last_refresh) >= refresh_ms)
        {
            last_refresh = now;

            (void)Monitor_GetTaskData(&task_count);
            task_pages = Monitor_TaskPages(task_count);
            pages = Monitor_PageCount(task_count);
            if (page >= pages) page = (uint8_t)(pages - 1U);

            if (page == 0U)
            {
                Monitor_DrawSummary(page, pages, task_count);
            }
            else if (page <= task_pages)
            {
                Monitor_DrawTasks(page, pages, task_count);
            }
            else if (page + 1U == pages)
            {
                Monitor_DrawDevices(page, pages);
            }
            else
            {
                if (queue_page_ready == 0U)
                {
                    Monitor_DrawQueues(page, pages);
                    queue_page_ready = 1U;
                    queue_slow_refresh = now;
                }
                else
                {
                    Monitor_RefreshQueueRow("CUR ", cursorHandle, 8U);

                    if ((now - queue_slow_refresh) >= 1000U)
                    {
                        Monitor_RefreshQueueRow("KEY ", KeyHandle, 16U);
                        Monitor_RefreshQueueRow("LOG ", LogQueueHandle, 24U);
                        queue_slow_refresh = now;
                    }
                }
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
                pages = Monitor_PageCount(count);

                if (key == '4')
                {
                    if (page > 0U) page--;
                }
                else if (page + 1U < pages)
                {
                    page++;
                }
                queue_page_ready = 0U;
                queue_slow_refresh = 0U;
                last_refresh = 0U;
            }
        }
        osDelay(10U);
    }
}
