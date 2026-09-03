/**
  ******************************************************************************
  * @file           : Watchdog_App.c
  * @brief          : IWDG watchdog task: heartbeat check + feed
  ******************************************************************************
  */

#include "Watchdog_App.h"
#include "TaskWatch.h"
#include "Log_App.h"
#include "main.h"
#include "cmsis_os.h"

static IWDG_HandleTypeDef watch_iwdg;

void Watchdog_Init(void)
{
    /* 开机检查上次是否为看门狗复位，写日志并清标志 */
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        Log_Write(LOG_TYPE_ERROR, "WDG RESET");
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();

    __HAL_RCC_LSI_ENABLE();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET) { }

    watch_iwdg.Instance = IWDG;
    watch_iwdg.Init.Prescaler = IWDG_PRESCALER_64;  /* LSI 40kHz / 64 = 625Hz */
    watch_iwdg.Init.Reload = 1250U;                 /* 1250 / 625 ≈ 2 秒 */
    (void)HAL_IWDG_Init(&watch_iwdg);
}

void Watchdog_Task_Sys(void)
{
    uint8_t reset_pending = 0U;

    Watchdog_Init();

    for (;;)
    {
        if (TaskWatch_AnyHang())
        {
            if (reset_pending)
            {
                /* 已确认异常并记录过：停止喂狗，等待 IWDG 复位 */
            }
            else
            {
                /* 首次确认异常：先喂狗并记录，下一轮仍异常再停喂 */
                TaskWatch_Check();
                if (TaskWatch_AnyHang())
                {
                    reset_pending = 1U;
                }
                HAL_IWDG_Refresh(&watch_iwdg);
            }
        }
        else
        {
            reset_pending = 0U;
            TaskWatch_Check();
            HAL_IWDG_Refresh(&watch_iwdg);
        }
        osDelay(200U);
    }
}

void Watchdog_Task(void *argument)
{
    (void)argument;
    Watchdog_Task_Sys();
}
