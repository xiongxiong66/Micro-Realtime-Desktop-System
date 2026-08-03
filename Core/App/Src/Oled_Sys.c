#include "Oled_Sys.h"
#include "Desktop_Sys.h"
#include "Set_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"
#include <string.h>

#define PIN_MAX_WRONG  3U
#define PIN_LOCK_SEC   10U

static void Oled_Lock_Sys(void)
{
    char key;

    /* Discard any keys queued during the previous input. */
    //清空Key消息队列
    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }

    for (uint8_t sec = PIN_LOCK_SEC; sec > 0U; sec--)
    {
        OLED_Clear();
        OLED_SetCursor(20, 23);
        OLED_PrintString("Lock:");
        OLED_SetCursor(56, 23);
        OLED_PrintNum(sec, 10);
        OLED_Display();
        osDelay(1000U);
    }

    /* Discard keys pressed while locked. */
    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }
}

static void Oled_Login_Sys(void)
{
    char pin_buf[SETTINGS_PIN_MAX_LEN + 1U] = {0};
    uint8_t pin_len = 0;
    uint8_t wrong_count = 0;
    char key;

  

    OLED_Clear();
    OLED_SetCursor(20, 23);
    OLED_PrintString("Pin:");
    OLED_Display();

    for (;;)
    {
        //阻塞等待
        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        if (key == '*')
        {
            if (pin_len > 0U)
            {
                pin_len--;
                pin_buf[pin_len] = '\0';
            }
        }
        else if (key == '#')
        {
            if (Set_Sys_CheckPin(pin_buf))
            {
                state = Statue_Yes;
                return;
            }

            pin_len = 0U;
            pin_buf[0] = '\0';

            OLED_Clear();
            OLED_SetCursor(20, 23);
            OLED_PrintString("Error!!!");
            OLED_Display();
            osDelay(1000U);

            //错误次数增加，进入锁定函数
            wrong_count++;
            if (wrong_count >= PIN_MAX_WRONG)
            {
                Oled_Lock_Sys();
                wrong_count = 0U;
            }
        }
        //在达到最大输出长度之前，按键输入到pin_buf中
        else if (pin_len < SETTINGS_PIN_MAX_LEN)
        {
            pin_buf[pin_len++] = key;
            pin_buf[pin_len] = '\0';
        }

        OLED_Clear();
        OLED_SetCursor(20, 23);
        OLED_PrintString("Pin:");
        OLED_SetCursor(48, 23);
        OLED_PrintString(pin_buf);
        OLED_Display();



    }
}

void Oled_Task_Sys(void)
{
    if (state == Statue_NO)
    {
        Oled_Login_Sys();
    }

    if (state == Statue_Yes)
    {
        Desktop_Sys_Run();
    }
}
