/**
  ******************************************************************************
  * @file           : NameEdit_App.c
  * @brief          : Shared keypad name editor
  ******************************************************************************
  */

#include "NameEdit_App.h"
#include "Oled_App.h"
#include "Screen_App.h"
#include "cmsis_os.h"
#include "oled.h"
#include <string.h>

uint8_t NameEdit_Run(char *name, uint8_t max_len, uint8_t prefill,
                     uint8_t (*check_dup)(const char *new_name, const char *old_name))
{
    char buf[12] = {0};
    uint8_t len = 0;
    char key;

    if (max_len > sizeof(buf)) max_len = (uint8_t)sizeof(buf);

    Screen_Sys_SetForceOff(0U);

    if (prefill && name)
    {
        memcpy(buf, name, max_len);
        while (len < max_len - 1U && buf[len] != '\0') len++;
        buf[len] = '\0';
    }

    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }

    for (;;)
    {
        OLED_Clear();
        OLED_SetCursor(0, 8);
        OLED_PrintString("Name:");
        OLED_SetCursor(0, 16);
        OLED_PrintString(buf);
        OLED_PrintChar('_');
        OLED_Display();

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D'))
        {
            if (len < max_len - 1U)
            {
                buf[len++] = key;
                buf[len] = '\0';
            }
        }
        else if (key == '*')
        {
            if (len > 0U)
            {
                len--;
                buf[len] = '\0';
            }
            else
            {
                Screen_Sys_SetForceOff(1U);
                return 0U;
            }
        }
        else if (key == '#')
        {
            if (len > 0U && name)
            {
                if (check_dup != NULL && check_dup(buf, name))
                {
                    OLED_Clear();
                    OLED_SetCursor(0, 24);
                    OLED_PrintString("DUP NAME");
                    OLED_Display();
                    osDelay(500U);
                    continue;
                }
                memcpy(name, buf, (size_t)len + 1U);
            }
            Screen_Sys_SetForceOff(1U);
            return 1U;
        }
    }
}
