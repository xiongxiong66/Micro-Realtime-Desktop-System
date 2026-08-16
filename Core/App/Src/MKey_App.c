/**
  ******************************************************************************
  * @file           : MKey_Sys.c
  * @brief          : Matrix keypad task, sends one key per press to UI tasks
  ******************************************************************************
  */

#include "MKey_Sys.h"
#include "mkey.h"
#include "keypad.h"
#include "Screen_Sys.h"

volatile uint32_t mkey_events = 0U;
volatile uint32_t mkey_dropped = 0U;

void MKey_Task_Sys(void)
{
    char key;

    BSP_Keypad_Init();

    for (;;)
    {
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

        osDelay(10U);
    }
}
