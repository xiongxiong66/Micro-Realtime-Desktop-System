/**
  ******************************************************************************
  * @file           : MKey_App.c
  * @brief          : Matrix keypad task, sends one key per press to UI tasks
  ******************************************************************************
  */

#include "MKey_App.h"
#include "TaskWatch.h"
#include "mkey.h"
#include "keypad.h"
#include "Screen_App.h"

#define MKEY_SCAN_PERIOD_MS       10U
#define MKEY_SCAN_PERIOD_SLEEP_MS 50U
#define MKEY_SCAN_PERIOD_DEEP_MS  200U

volatile uint32_t mkey_events = 0U;
volatile uint32_t mkey_dropped = 0U;

void MKey_Task_Sys(void)
{
    char key;

    BSP_Keypad_Init();

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_MKEY);
        TaskWatch_Check();

        Screen_Sys_Update();

        if (MKey_GetKeyEvent(&key) == MKEY_OK)
        {
            if (key == 'A' && Screen_Sys_ForceOffEnabled())
            {
                Screen_Sys_ForceOff();
            }
            else
            {
                Screen_Sys_Wake();
                if (osMessageQueuePut(KeyHandle, &key, 0U, 0U) == osOK) mkey_events++;
                else mkey_dropped++;
            }
        }

        osDelay(Screen_Sys_IsOff() ?
                (Screen_Sys_IsDeepOff() ? MKEY_SCAN_PERIOD_DEEP_MS : MKEY_SCAN_PERIOD_SLEEP_MS) :
                MKEY_SCAN_PERIOD_MS);
    }
}
