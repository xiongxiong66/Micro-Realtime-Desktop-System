/**
  ******************************************************************************
  * @file           : Desktop_Sys.c
  * @brief          : Desktop main interface and app switching
  ******************************************************************************
  */

#include "Desktop_Sys.h"
#include "Oled_Sys.h"
#include "Sw_Adc_Sys.h"
#include "MKey_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define DESKTOP_GRID_COLS   3U
#define DESKTOP_GRID_ROWS   2U
#define DESKTOP_CELL_W      42U
#define DESKTOP_CELL_H      24U
#define DESKTOP_GRID_Y      8U
#define DESKTOP_CELL_W_IN   40U
#define DESKTOP_CELL_H_IN   22U

#define DESKTOP_STEP        6U
#define DESKTOP_IN_TIMEOUT  1000U
#define DESKTOP_STACK_WORDS 512U

typedef enum {
    APP_FILE = 0,
    APP_DRAW,
    APP_MUSIC,
    APP_LOG,
    APP_MONITOR,
    APP_SETTINGS,
    APP_COUNT           //应用总数，用作光标判断的无效命中
} AppId_t;

static const char app_names[APP_COUNT][5] = {
    "FILE", "DRAW", "MUSI", "LOG", "MON", "SET"
};

static uint32_t desktop_err_count = 0U;
//@brief:在指定图标格子居中打印应用名称
static void Desktop_PrintLabel(uint8_t x, uint8_t y, AppId_t app, uint8_t color)
{
    const char *name = app_names[app];
    uint8_t len = 0;
    //获取应用图标有效字符长度
    while (name[len] != '\0') len++;
    //居中打印+边框留白
    OLED_PrintStringColor((uint8_t)(x + 1U + (DESKTOP_CELL_W_IN - len * 6U) / 2U),
                          (uint8_t)(y + 1U + (DESKTOP_CELL_H_IN - 8U) / 2U),
                          name, color);
}

static AppId_t Desktop_HitTest(const CursorMsg_t *cur)
{
    uint8_t row, col;

    if (cur->cursor_x < 0 || cur->cursor_y < (int16_t)DESKTOP_GRID_Y) return APP_COUNT;
    if (cur->cursor_y >= (int16_t)(DESKTOP_GRID_Y + DESKTOP_GRID_ROWS * DESKTOP_CELL_H)) return APP_COUNT;

    row = (uint8_t)((cur->cursor_y - (int16_t)DESKTOP_GRID_Y) / DESKTOP_CELL_H);
    col = (uint8_t)(cur->cursor_x / DESKTOP_CELL_W);
    if (row >= DESKTOP_GRID_ROWS || col >= DESKTOP_GRID_COLS) return APP_COUNT;

    return (AppId_t)(row * DESKTOP_GRID_COLS + col);                //row为0——1，col为0——2，返回值为0——5，正好对应应用ID
}

static void Desktop_Draw(const CursorMsg_t *cur, uint8_t input_alive)
{
    int16_t cx, cy;

    OLED_Clear();

    OLED_SetCursor(0, 0);
    OLED_PrintString(input_alive ? "In:ON" : "In:NO");
    OLED_SetCursor(98, 0);
    OLED_PrintString("E:");
    OLED_PrintNum(desktop_err_count, 10);

    for (uint8_t r = 0; r < DESKTOP_GRID_ROWS; r++)
    {
        for (uint8_t c = 0; c < DESKTOP_GRID_COLS; c++)
        {
            AppId_t app = (AppId_t)(r * DESKTOP_GRID_COLS + c);
            uint8_t x = (uint8_t)(c * DESKTOP_CELL_W);
            uint8_t y = (uint8_t)(DESKTOP_GRID_Y + r * DESKTOP_CELL_H);

            if (app == Desktop_HitTest(cur))
            {
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U),
                              DESKTOP_CELL_W_IN, DESKTOP_CELL_H_IN, OLED_WHITE);
                Desktop_PrintLabel(x, y, app, OLED_BLACK);
            }
            else
            {
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U), DESKTOP_CELL_W_IN, 1U, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + DESKTOP_CELL_H_IN), DESKTOP_CELL_W_IN, 1U, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U), 1U, DESKTOP_CELL_H_IN, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + DESKTOP_CELL_W_IN), (uint8_t)(y + 1U), 1U, DESKTOP_CELL_H_IN, OLED_WHITE);
                Desktop_PrintLabel(x, y, app, OLED_WHITE);
            }
        }
    }

    cx = cur->cursor_x;
    cy = cur->cursor_y;
    if (cx < 0) cx = 0;
    if (cx > (int16_t)(OLED_WIDTH - 2)) cx = (int16_t)(OLED_WIDTH - 2);
    if (cy < 0) cy = 0;
    if (cy > (int16_t)(OLED_HEIGHT - 2)) cy = (int16_t)(OLED_HEIGHT - 2);
    OLED_DrawPixel((uint8_t)cx, (uint8_t)cy, OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx + 1), (uint8_t)cy, OLED_WHITE);
    OLED_DrawPixel((uint8_t)cx, (uint8_t)(cy + 1), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx + 1), (uint8_t)(cy + 1), OLED_WHITE);

    OLED_SetCursor(0, 56);
    OLED_PrintString("X:");
    OLED_PrintNum((uint32_t)cur->cursor_x, 10);
    OLED_PrintString(" Y:");
    OLED_PrintNum((uint32_t)cur->cursor_y, 10);

    OLED_Display();
}

static void Oled_App_Monitor_Draw(void)
{
    uint32_t sec = HAL_GetTick() / 1000U;
    uint32_t hh = sec / 3600U;
    uint32_t mm = (sec / 60U) % 60U;
    uint32_t ss = sec % 60U;
    uint32_t free_heap = xPortGetFreeHeapSize();
    UBaseType_t stack_water = uxTaskGetStackHighWaterMark(NULL);
    uint32_t stack_used = 100U;

    if (stack_water <= DESKTOP_STACK_WORDS)
    {
        stack_used = 100U - (uint32_t)stack_water * 100U / DESKTOP_STACK_WORDS;
    }

    OLED_Clear();

    OLED_SetCursor(0, 0);
    OLED_PrintString("UP ");
    OLED_PrintNum(hh, 10);
    OLED_PrintChar(':');
    if (mm < 10U) OLED_PrintChar('0');
    OLED_PrintNum(mm, 10);
    OLED_PrintChar(':');
    if (ss < 10U) OLED_PrintChar('0');
    OLED_PrintNum(ss, 10);

    OLED_SetCursor(0, 8);
    OLED_PrintString("HEAP ");
    OLED_PrintNum(free_heap, 10);

    OLED_SetCursor(0, 16);
    OLED_PrintString("EV ");
    OLED_PrintNum(adc_events + mkey_events, 10);
    OLED_PrintString(" DRP ");
    OLED_PrintNum(adc_dropped + mkey_dropped, 10);

    OLED_SetCursor(0, 24);
    OLED_PrintString("Q K");
    OLED_PrintNum(uxQueueMessagesWaiting(KeyHandle), 10);
    OLED_PrintString(" C");
    OLED_PrintNum(uxQueueMessagesWaiting(cursorHandle), 10);

    OLED_SetCursor(0, 32);
    OLED_PrintString("ERR ");
    OLED_PrintNum(desktop_err_count, 10);

    OLED_SetCursor(0, 40);
    OLED_PrintString("STK ");
    OLED_PrintNum(stack_used, 10);
    OLED_PrintChar('%');

    OLED_SetCursor(0, 56);
    OLED_PrintString("*:back");

    OLED_Display();
}

static void Oled_App_Monitor(void)
{
    char key;
    uint32_t last_refresh = 0U;

    for (;;)
    {
        uint32_t now = HAL_GetTick();

        if ((now - last_refresh) >= 1000U)
        {
            last_refresh = now;
            Oled_App_Monitor_Draw();
        }

        if (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK)
        {
            if (key == '*') return;
        }
        osDelay(10U);
    }
}

static void Oled_App_Placeholder(AppId_t app)
{
    char key;

    OLED_Clear();
    OLED_SetCursor(20, 16);
    OLED_PrintString(app_names[app]);
    OLED_SetCursor(20, 32);
    OLED_PrintString("*:exit");
    OLED_Display();

    for (;;)
    {
        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) == osOK)
        {
            if (key == '*') return;
        }
    }
}

static void Oled_App_Run(AppId_t app)
{
    if (app == APP_MONITOR)
    {
        Oled_App_Monitor();
    }
    else
    {
        Oled_App_Placeholder(app);
    }
}

void Desktop_Sys_Run(void)
{
    CursorMsg_t cur = {64, 32, 0};
    uint8_t input_alive = 1U;
    uint8_t prev_button = 0U;
    uint8_t need_redraw = 1U;
    uint32_t last_cursor_tick = HAL_GetTick();
    char key;

    for (;;)
    {
        uint8_t changed = 0U;

        while (osMessageQueueGet(cursorHandle, &cur, NULL, 0U) == osOK)
        {
            last_cursor_tick = HAL_GetTick();
            input_alive = 1U;
            changed = 1U;
        }

        if (input_alive && (HAL_GetTick() - last_cursor_tick) >= DESKTOP_IN_TIMEOUT)
        {
            input_alive = 0U;
            changed = 1U;
        }

        while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK)
        {
            switch (key)
            {
                case '2':
                    cur.cursor_y = (cur.cursor_y > (int16_t)DESKTOP_STEP)
                                 ? (int16_t)(cur.cursor_y - DESKTOP_STEP) : 0;
                    changed = 1U;
                    break;
                case '8':
                    cur.cursor_y = (cur.cursor_y + (int16_t)DESKTOP_STEP > (int16_t)(OLED_HEIGHT - 1))
                                 ? (int16_t)(OLED_HEIGHT - 1)
                                 : (int16_t)(cur.cursor_y + DESKTOP_STEP);
                    changed = 1U;
                    break;
                case '4':
                    cur.cursor_x = (cur.cursor_x > (int16_t)DESKTOP_STEP)
                                 ? (int16_t)(cur.cursor_x - DESKTOP_STEP) : 0;
                    changed = 1U;
                    break;
                case '6':
                    cur.cursor_x = (cur.cursor_x + (int16_t)DESKTOP_STEP > (int16_t)(OLED_WIDTH - 1))
                                 ? (int16_t)(OLED_WIDTH - 1)
                                 : (int16_t)(cur.cursor_x + DESKTOP_STEP);
                    changed = 1U;
                    break;
                case '5':
                case '#':
                {
                    AppId_t app = Desktop_HitTest(&cur);
                    if (app != APP_COUNT)
                    {
                        Oled_App_Run(app);
                        need_redraw = 1U;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (cur.button_pressed != 0U && prev_button == 0U)
        {
            AppId_t app = Desktop_HitTest(&cur);
            if (app != APP_COUNT)
            {
                Oled_App_Run(app);      //进入对应app死循环
                need_redraw = 1U;       //退出时重新绘制桌面
            }
        }
        prev_button = (cur.button_pressed != 0U) ? 1U : 0U;

        if (changed || need_redraw)
        {
            Desktop_Draw(&cur, input_alive);
            need_redraw = 0U;
        }

        osDelay(10U);
    }
}
