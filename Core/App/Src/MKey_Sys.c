/**
  ******************************************************************************
  * @file           : MKey_Sys.c
  * @brief          : Matrix keypad task, sends one key per press to OLED task
  ******************************************************************************
  */

#include "MKey_Sys.h"
#include "mkey.h"
#include "keypad.h"

void MKey_Task_Sys(void)
{
    char key;

    BSP_Keypad_Init();

    for (;;)
    {
        if (MKey_GetKeyEvent(&key) == MKEY_OK)
        {
            osMessageQueuePut(KeyHandle, &key, 0U, 0U);
        }
        osDelay(10U);
    }
}
