/**
  ******************************************************************************
  * @file           : NameEdit_Sys.c
  * @brief          : Shared keypad name editor
  ******************************************************************************
  */

#include "NameEdit_Sys.h"
#include "Oled_Sys.h"
#include "Screen_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include <string.h>

uint8_t NameEdit_Run(char *name, uint8_t max_len, uint8_t prefill)
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
        OLED_SetCursor(0, 48);
        OLED_PrintString("#:OK  *:DEL");
        OLED_SetCursor(0, 56);
        OLED_PrintString("empty+*:back");
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
                memcpy(name, buf, (size_t)len + 1U);
            }
            Screen_Sys_SetForceOff(1U);
            return 1U;
        }
    }
}
