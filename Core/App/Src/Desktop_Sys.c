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
#include "Draw_Sys.h"
#include "File_Sys.h"
#include "Music_Sys.h"
#include "Monitor_Sys.h"
#include "Set_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"

#define DESKTOP_GRID_COLS   3U
#define DESKTOP_GRID_ROWS   2U
#define DESKTOP_CELL_W      42U
#define DESKTOP_CELL_H      24U
#define DESKTOP_GRID_Y      8U
#define DESKTOP_CELL_W_IN   40U
#define DESKTOP_CELL_H_IN   22U

#define DESKTOP_HYST        4U
#define DESKTOP_IN_TIMEOUT  1000U

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

/* æ»åéä¸­ï¼åæ åå¨æ ¼è¾¹çéè¿æ¶ä¿æåéä¸­ï¼é¿åé«äº®å·¦å³è·³å¨ */
static AppId_t Desktop_SelectApp(const CursorMsg_t *cur, AppId_t last)
{
    AppId_t hit = Desktop_HitTest(cur);

    if (hit == last) return hit;
    if (last == APP_COUNT || hit == APP_COUNT) return hit;

    {
        int16_t lr = (int16_t)(last / DESKTOP_GRID_COLS);
        int16_t lc = (int16_t)(last % DESKTOP_GRID_COLS);
        int16_t hr = (int16_t)(hit / DESKTOP_GRID_COLS);
        int16_t hc = (int16_t)(hit % DESKTOP_GRID_COLS);

        if (hc != lc)
        {
            int16_t bx = (int16_t)((hc > lc ? hc : lc) * DESKTOP_CELL_W);
            if (hc > lc && cur->cursor_x < bx + (int16_t)DESKTOP_HYST) return last;
            if (hc < lc && cur->cursor_x > bx - (int16_t)DESKTOP_HYST) return last;
        }
        if (hr != lr)
        {
            int16_t by = (int16_t)(DESKTOP_GRID_Y + (hr > lr ? hr : lr) * DESKTOP_CELL_H);
            if (hr > lr && cur->cursor_y < by + (int16_t)DESKTOP_HYST) return last;
            if (hr < lr && cur->cursor_y > by - (int16_t)DESKTOP_HYST) return last;
        }
    }

    return hit;
}

static void Desktop_Draw(const CursorMsg_t *cur, uint8_t input_alive, AppId_t sel)
{
    int16_t cx, cy;

    OLED_Clear();

    OLED_SetCursor(0, 0);
    OLED_PrintString(input_alive ? "In:ON" : "In:NO");
    OLED_FillRect(98, 0, 28, 8, OLED_WHITE);
    OLED_PrintStringColor(100, 0, Music_Bg_IsPlaying() ? "1:||" : "1:>", OLED_BLACK);

    for (uint8_t r = 0; r < DESKTOP_GRID_ROWS; r++)
    {
        for (uint8_t c = 0; c < DESKTOP_GRID_COLS; c++)
        {
            AppId_t app = (AppId_t)(r * DESKTOP_GRID_COLS + c);
            uint8_t x = (uint8_t)(c * DESKTOP_CELL_W);
            uint8_t y = (uint8_t)(DESKTOP_GRID_Y + r * DESKTOP_CELL_H);

            if (app == sel)
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
    uint8_t cs = (uint8_t)(Set_Sys_GetCursorSize() + 1U);
    if (cx < 0) cx = 0;
    if (cx > (int16_t)(OLED_WIDTH - cs)) cx = (int16_t)(OLED_WIDTH - cs);
    if (cy < 0) cy = 0;
    if (cy > (int16_t)(OLED_HEIGHT - cs)) cy = (int16_t)(OLED_HEIGHT - cs);
    for (uint8_t yy = 0U; yy < cs; yy++)
    {
        for (uint8_t xx = 0U; xx < cs; xx++)
        {
            OLED_DrawPixel((uint8_t)(cx + xx), (uint8_t)(cy + yy), OLED_WHITE);
        }
    }

    OLED_SetCursor(0, 56);
    OLED_PrintString("X:");
    OLED_PrintNum((uint32_t)cur->cursor_x, 10);
    OLED_PrintString(" Y:");
    OLED_PrintNum((uint32_t)cur->cursor_y, 10);

    OLED_Display();
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
            else if (key == '1') Music_Bg_Toggle();
        }
    }
}

static void Oled_App_Run(AppId_t app)
{
    if (app == APP_MONITOR)
    {
        Cursor_Suspend();
        Monitor_Sys_Run();
        Cursor_Resume();
    }
    else if (app == APP_DRAW)
    {
        osThreadResume(App_DrawHandle);
        osThreadSuspend(oledHandle);
    }
    else if (app == APP_FILE)
    {
        Cursor_Suspend();
        osThreadResume(App_FileHandle);
        osThreadSuspend(oledHandle);
        Cursor_Resume();
    }
    else if (app == APP_MUSIC)
    {
        Cursor_Suspend();
        osThreadResume(App_MusicHandle);
        osThreadSuspend(oledHandle);
        Cursor_Resume();
    }
    else if (app == APP_SETTINGS)
    {
        Cursor_Suspend();
        Set_Sys_Run();
        Cursor_Resume();
    }
    else
    {
        Cursor_Suspend();
        Oled_App_Placeholder(app);
        Cursor_Resume();
    }
}

void Desktop_Sys_Run(void)
{
    CursorMsg_t cur = {64, 32, 0};
    uint8_t input_alive = 1U;
    uint8_t prev_button = 0U;
    uint8_t need_redraw = 1U;
    uint32_t last_cursor_tick = HAL_GetTick();
    int16_t last_x = cur.cursor_x;
    int16_t last_y = cur.cursor_y;
    uint8_t last_button = cur.button_pressed;
    AppId_t sel = APP_COUNT;
    char key;

    for (;;)
    {
        uint8_t changed = 0U;

        while (osMessageQueueGet(cursorHandle, &cur, NULL, 0U) == osOK)
        {
            last_cursor_tick = HAL_GetTick();
            if (cur.cursor_x != last_x || cur.cursor_y != last_y
             || cur.button_pressed != last_button || !input_alive)
            {
                changed = 1U;
            }
            input_alive = 1U;
        }

        if (input_alive && (HAL_GetTick() - last_cursor_tick) >= DESKTOP_IN_TIMEOUT)
        {
            input_alive = 0U;
            changed = 1U;
        }

        sel = Desktop_SelectApp(&cur, sel);

        while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK)
        {
            if (key == '#')
            {
                if (sel != APP_COUNT)
                {
                    Oled_App_Run(sel);
                    need_redraw = 1U;
                }
            }
            else if (key == '1')
            {
                Music_Bg_Toggle();
                need_redraw = 1U;
            }
        }

        if (cur.button_pressed != 0U && prev_button == 0U)
        {
            AppId_t app = sel;
            if (app != APP_COUNT)
            {
                Oled_App_Run(app);      //进入对应app死循环
                need_redraw = 1U;       //退出时重新绘制桌面
            }
        }
        prev_button = (cur.button_pressed != 0U) ? 1U : 0U;

        if (changed || need_redraw)
        {
            Desktop_Draw(&cur, input_alive, sel);
            need_redraw = 0U;
        }

        last_x = cur.cursor_x;
        last_y = cur.cursor_y;
        last_button = cur.button_pressed;

        osDelay(10U);
    }
}
