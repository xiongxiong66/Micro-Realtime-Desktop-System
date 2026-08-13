#include "Oled_Sys.h"
#include "Desktop_Sys.h"
#include "Set_Sys.h"
#include "Log_Sys.h"
#include "Screen_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"
#include <string.h>

#define PIN_MAX_WRONG  3U       //错误次数达到3次后锁定
#define PIN_LOCK_SEC   10U      //锁定时间10s
//@brief:锁定界面，显示倒计时，期间丢弃所有按键输入
static void Oled_Lock_Sys(void)
{
    char key;

    /* Discard any keys queued during the previous input. */
    //清空Key消息队列，队列有数据就进入空循环，没有则退出
    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }
    //10s锁定
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
    //清空key消息队列，队列有数据就进入空循环，没有则退出
    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }
}

static void Oled_Login_Sys(void)
{
    char pin_buf[SETTINGS_PIN_MAX_LEN + 1U] = {0};      //密码缓冲区，+1是为了存放字符串结束符'\0'
    uint8_t pin_len = 0;                                //密码长度
    uint8_t wrong_count = 0;                            //错误次数计数器
    CursorMsg_t cur;
    char key;                                           //按键输入缓冲区

  

    OLED_Clear();
    OLED_SetCursor(20, 23);
    OLED_PrintString("Pin:");
    OLED_Display();

    for (;;)
    {
        //阻塞等待，如果队列中没有数据，则一直等待，直到有数据为止
        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }
        //如果按下了*键，删除最后一个字符
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
               //密码正确，清空光标消息队列
                while (osMessageQueueGet(cursorHandle, &cur, NULL, 0U) == osOK) { }
                state = Statue_Yes;
                return;
            }
            //密码错误，记录日志，清空输入缓冲区，显示错误提示，增加错误次数计数器，如果达到最大错误次数，则进入锁定函数
            Log_Write(LOG_TYPE_ERROR, "LOGIN FAIL");

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
                //达到最大错误次数，进入锁定函数
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
    OLED_Clear();
    OLED_SetCursor(49, 28);
    OLED_PrintString("START");
    OLED_Display();
    osDelay(1000U);

    Screen_Sys_SetForceOff(0U);

    if (state == Statue_NO)
    {
        Oled_Login_Sys();
    }

    Screen_Sys_SetForceOff(1U);

    if (state == Statue_Yes)
    {
        Desktop_Sys_Run();
    }
}
