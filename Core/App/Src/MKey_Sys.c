/**
  ******************************************************************************
  * @file           : MKey_Sys.c
  * @brief          : Matrix keypad task, sends one key per press to OLED task
  ******************************************************************************
  */

#include "MKey_Sys.h"
#include "mkey.h"
#include "keypad.h"

volatile uint32_t mkey_events = 0U;
volatile uint32_t mkey_dropped = 0U;

void MKey_Task_Sys(void)
{
    char key;

    BSP_Keypad_Init();

    for (;;)
    {
        if (MKey_GetKeyEvent(&key) == MKEY_OK)
        {
            if (osMessageQueuePut(KeyHandle, &key, 0U, 0U) == osOK) mkey_events++;
            else mkey_dropped++;
        }
        osDelay(10U);
    }
}
