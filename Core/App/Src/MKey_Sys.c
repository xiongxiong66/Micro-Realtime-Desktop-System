/**
  ******************************************************************************
  * @file           : MKey_Sys.c
  * @brief          : Matrix keypad task, sends one key per press to OLED task
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
        //息屏检测
        Screen_Sys_Update();

        if (MKey_GetKeyEvent(&key) == MKEY_OK)
        {   
            //唤醒屏幕
            Screen_Sys_Wake();
            //对输入与丢失进行计数，再送到monitor应用中显示
            if (osMessageQueuePut(KeyHandle, &key, 0U, 0U) == osOK) mkey_events++;
            else mkey_dropped++;
        }
        osDelay(10U);
    }
}
