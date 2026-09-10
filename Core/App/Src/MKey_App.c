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
#include "TaskSnap.h"

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

        Screen_Sys_Update();

        if (MKey_GetKeyEvent(&key) == MKEY_OK)
        {
            if (key == 'A' && Screen_Sys_ForceOffEnabled())
            {
                Screen_Sys_ForceOff();
            }
            else
            {
                uint8_t was_off = Screen_Sys_IsOff();

                Screen_Sys_Wake();
                if (was_off == 0U && key == 'B' && Screen_Sys_ForceOffEnabled())
                {
                    (void)TaskSnap_Record();
                }
                else if (was_off == 0U && key == '9' && Screen_Sys_ForceOffEnabled())
                {
                    TaskWatch_DebugRequestCurrentUi();
                    TaskWatch_Check();
                    if (osMessageQueuePut(KeyHandle, &key, 0U, 0U) == osOK)
                    {
                        mkey_events++;
                    }
                    else
                    {
                        mkey_dropped++;
                    }
                }
                else if (was_off != 0U && Screen_Sys_IsLocking())
                {
                    /* 唤醒按键只负责点亮/进入锁屏，不投递给登录前的应用 */
                }
                else if (osMessageQueuePut(KeyHandle, &key, 0U, 0U) == osOK)
                {
                    mkey_events++;
                }
                else
                {
                    mkey_dropped++;
                }
            }
        }

        osDelay(Screen_Sys_IsOff() ?
                (Screen_Sys_IsDeepOff() ? MKEY_SCAN_PERIOD_DEEP_MS : MKEY_SCAN_PERIOD_SLEEP_MS) :
                MKEY_SCAN_PERIOD_MS);
    }
}
