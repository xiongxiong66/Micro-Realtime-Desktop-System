#include "Oled_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"
#include <string.h>

#define PIN_MAX_LEN    10U
#define PIN_PASSWORD   "12345"
#define PIN_MAX_WRONG  3U
#define PIN_LOCK_SEC   10U

static void Oled_Lock_Sys(void)
{
    char key;

    /* Discard any keys queued during the previous input. */
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
    char pin_buf[PIN_MAX_LEN + 1U] = {0};
    uint8_t pin_len = 0;
    uint8_t wrong_count = 0;
    char key;

  

    OLED_Clear();
    OLED_SetCursor(20, 23);
    OLED_PrintString("Pin:");
    OLED_Display();

    for (;;)
    {
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
            if (strcmp(pin_buf, PIN_PASSWORD) == 0)
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

            wrong_count++;
            if (wrong_count >= PIN_MAX_WRONG)
            {
                Oled_Lock_Sys();
                wrong_count = 0U;
            }
        }
        else if (pin_len < PIN_MAX_LEN)
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

static void Oled_Cursor_Sys(void)
{
    CursorMsg_t msg;

    for (;;)
    {
        if (osMessageQueueGet(cursorHandle, &msg, NULL, 0U) == osOK)
        {
            int16_t cx = (int16_t)msg.cursor_x;
            int16_t cy = (int16_t)msg.cursor_y;

            OLED_Clear();

            for (int16_t dy = -3; dy <= 3; dy++)
            {
                for (int16_t dx = -3; dx <= 3; dx++)
                {
                    int16_t px = cx + dx;
                    int16_t py = cy + dy;
                    if (dx * dx + dy * dy <= 9
                     && px >= 0 && px < (int16_t)OLED_WIDTH
                     && py >= 0 && py < (int16_t)OLED_HEIGHT)
                    {
                        OLED_DrawPixel((uint8_t)px, (uint8_t)py, OLED_WHITE);
                    }
                }
            }

            OLED_SetCursor(0, 56);
            OLED_PrintString("X:");
            OLED_PrintNum((uint32_t)msg.cursor_x, 10);
            OLED_PrintString(" Y:");
            OLED_PrintNum((uint32_t)msg.cursor_y, 10);

            OLED_Display();
        }
        osDelay(15U);
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
        Oled_Cursor_Sys();
    }
}
