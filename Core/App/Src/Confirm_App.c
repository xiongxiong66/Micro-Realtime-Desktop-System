/**
  ******************************************************************************
  * @file           : Confirm_App.c
  * @brief          : Shared delete confirm dialog
  ******************************************************************************
  */

#include "Confirm_App.h"
#include "Oled_App.h"
#include "cmsis_os.h"
#include "oled.h"
//@brief:显示删除确认对话框，name为要删除的文件名，返回值为1U表示确认删除，0U表示取消删除
uint8_t Confirm_Delete(const char *name)
{
    char title[24];
    uint8_t i = 0;
    uint8_t sel = 1U;   /* default No */
    char key;
    static const char prefix[] = "Delete ";

    while (prefix[i] != '\0')
    {
        title[i] = prefix[i];
        i++;
    }
    if (name)
    {
        for (uint8_t k = 0; k < 11U && name[k] != '\0'; k++)
        {
            title[i++] = name[k];
        }
    }
    title[i] = '\0';

    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }

    for (;;)
    {
        OLED_Clear();
        OLED_SetCursor(0, 8);
        OLED_PrintString(title);
        OLED_SetCursor(0, 40);
        if (sel == 0U) OLED_PrintString("> ");
        OLED_PrintString("Yes");
        OLED_SetCursor(70, 40);
        if (sel == 1U) OLED_PrintString("> ");
        OLED_PrintString("No");
        OLED_SetCursor(0, 56);
        OLED_PrintString("4/6:#:OK *:BACK");
        OLED_Display();

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        if (key == '4' || key == '6')
        {
            sel ^= 1U;
        }
        else if (key == '#')
        {
            return (sel == 0U) ? 1U : 0U;
        }
        else if (key == '*')
        {
            return 0U;
        }
    }
}
