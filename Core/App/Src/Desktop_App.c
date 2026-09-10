/**
  ******************************************************************************
  * @file           : Desktop_App.c
  * @brief          : Desktop main interface and app switching
  ******************************************************************************
  */

#include "Desktop_App.h"
#include "TaskWatch.h"
#include "InputDev_App.h"
#include "Oled_App.h"
#include "Sw_Adc_App.h"
#include "MKey_App.h"
#include "Draw_App.h"
#include "File_App.h"
#include "Music_App.h"
#include "Monitor_App.h"
#include "Set_App.h"
#include "Log_App.h"
#include "DS3231.h"
#include "cmsis_os.h"
#include "oled.h"
#include "main.h"

#define DESKTOP_GRID_COLS   3U      //桌面图标格子列数
#define DESKTOP_GRID_ROWS   2U      //桌面图标格子行数
#define DESKTOP_CELL_W      42U     //每个图标格子宽度
#define DESKTOP_CELL_H      24U     //每个图标格子高度
#define DESKTOP_GRID_Y      8U
#define DESKTOP_CELL_W_IN   40U
#define DESKTOP_CELL_H_IN   22U

#define DESKTOP_CORNER_SIZE 4U
#define DESKTOP_ERR_BLINK_MS 4000U

static uint32_t desk_err_count = 0U;
static uint32_t desk_err_blink_until = 0U;

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

static void Desktop_Print2(uint32_t value)
{
    if (value < 10U) OLED_PrintChar('0');
    OLED_PrintNum(value, 10);
}

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
//@brief:光标命中测试，返回命中的应用ID，如果没有命中则返回APP_COUNT
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

//@brief:根据光标位置返回当前命中的应用ID，如果没有命中则返回APP_COUNT
static AppId_t Desktop_SelectApp(const CursorMsg_t *cur, AppId_t last)
{
    (void)last;
    return Desktop_HitTest(cur);
}
//@brief:绘制桌面界面，包括应用图标格子、光标和状态栏
static void Desktop_Draw(const CursorMsg_t *cur, AppId_t sel)
{
    int16_t cx, cy;

    OLED_Clear();

    OLED_PrintStringColor(0, 0, InputDev_IsConnected() ? "IN YES" : "IN NO", OLED_WHITE);

    {
        uint32_t err = Monitor_Sys_GetErrorCount();
        uint8_t show = 1U;

        if (err != desk_err_count)
        {
            desk_err_count = err;
            desk_err_blink_until = HAL_GetTick() + DESKTOP_ERR_BLINK_MS;
        }
        if (show != 0U && HAL_GetTick() < desk_err_blink_until
         && ((HAL_GetTick() / 500U) & 1U) != 0U)
        {
            show = 0U;
        }
        if (show != 0U)
        {
            OLED_SetCursor(50, 0);
            OLED_PrintString("ERR ");
            OLED_PrintNum(err, 10);
        }
    }

    OLED_FillRect(98, 0, 28, 8, OLED_BLACK);
    OLED_PrintStringColor(100, 0, Music_Bg_IsPlaying() ? "||" : "|>", OLED_WHITE);

    for (uint8_t r = 0; r < DESKTOP_GRID_ROWS; r++)
    {
        for (uint8_t c = 0; c < DESKTOP_GRID_COLS; c++)
        {
            AppId_t app = (AppId_t)(r * DESKTOP_GRID_COLS + c);
            uint8_t x = (uint8_t)(c * DESKTOP_CELL_W);
            uint8_t y = (uint8_t)(DESKTOP_GRID_Y + r * DESKTOP_CELL_H);

            if (app == sel)
            {
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U), DESKTOP_CELL_W_IN, 1U, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + DESKTOP_CELL_H_IN), DESKTOP_CELL_W_IN, 1U, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U), 1U, DESKTOP_CELL_H_IN, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + DESKTOP_CELL_W_IN), (uint8_t)(y + 1U), 1U, DESKTOP_CELL_H_IN, OLED_WHITE);
                Desktop_PrintLabel(x, y, app, OLED_WHITE);

                OLED_FillRect((uint8_t)(x + 2U), (uint8_t)(y + 2U),
                              DESKTOP_CORNER_SIZE, DESKTOP_CORNER_SIZE, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + DESKTOP_CELL_W_IN - 2U - DESKTOP_CORNER_SIZE + 1U),
                              (uint8_t)(y + 2U),
                              DESKTOP_CORNER_SIZE, DESKTOP_CORNER_SIZE, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + 2U),
                              (uint8_t)(y + DESKTOP_CELL_H_IN - 2U - DESKTOP_CORNER_SIZE + 1U),
                              DESKTOP_CORNER_SIZE, DESKTOP_CORNER_SIZE, OLED_WHITE);
                OLED_FillRect((uint8_t)(x + DESKTOP_CELL_W_IN - 2U - DESKTOP_CORNER_SIZE + 1U),
                              (uint8_t)(y + DESKTOP_CELL_H_IN - 2U - DESKTOP_CORNER_SIZE + 1U),
                              DESKTOP_CORNER_SIZE, DESKTOP_CORNER_SIZE, OLED_WHITE);
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
    uint8_t cs = Set_Sys_GetCursorPixels();
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
    {
        DS3231_Time_t rtc;

        if (DS3231_ReadTime(&rtc))
        {
            Desktop_Print2((uint32_t)rtc.year);
            OLED_PrintChar(':');
            OLED_PrintNum((uint32_t)rtc.month, 10);
            OLED_PrintChar(':');
            OLED_PrintNum((uint32_t)rtc.day, 10);
            OLED_PrintChar(':');
            OLED_PrintNum((uint32_t)rtc.hour, 10);
            OLED_PrintChar(':');
            Desktop_Print2((uint32_t)rtc.minute);
            OLED_PrintChar(':');
            Desktop_Print2((uint32_t)rtc.second);
        }
        else
        {
            OLED_PrintString("--:--:--");
        }
    }

    OLED_Display();
}

//@brief:根据应用ID，进入对应的应用死循环，如果任务需要后台运行，则在应用中自行挂起oled任务，退出时再恢复oled任务，如果不需要后台运行，则直接在应用中运行死循环，退出时返回桌面
static void Oled_App_Run(AppId_t app)
{
    if (app == APP_MONITOR)
    {
        Monitor_Sys_Run();
    }
    else if (app == APP_DRAW)
    {
        Draw_Sys_Run();
    }
    else if (app == APP_FILE)
    {
        Cursor_Suspend();
        osThreadResume(App_FileHandle);
        TaskWatch_Refresh(TASKWATCH_FILE);
        osThreadSuspend(oledHandle);
        Cursor_Resume();
    }
    else if (app == APP_MUSIC)
    {
        Cursor_Suspend();
        Music_App_Run();
        Cursor_Resume();
    }
    else if (app == APP_SETTINGS)
    {
        Cursor_Suspend();
        Set_Sys_Run();
        Cursor_Resume();
    }
    else if (app == APP_LOG)
    {
        Log_View_Run();
    }

}
//@brief:桌面主循环，处理光标输入、应用切换和状态栏显示，光标按键与矩阵按键都可以进入应用
void Desktop_Sys_Run(void)
{
    CursorMsg_t cur = {64, 32, 0};
    uint8_t prev_button = 0U;
    uint8_t need_redraw = 1U;
    uint32_t last_time_tick = HAL_GetTick();
    int16_t last_x = cur.cursor_x;
    int16_t last_y = cur.cursor_y;
    uint8_t last_button = cur.button_pressed;
    AppId_t sel = APP_COUNT;
    char key;

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_OLED);

        uint8_t changed = 0U;
        //读到队列最后一个数据，如果光标按键的输入变化，就设置changed=1U，表示需要重新绘制桌面界面，无论有没有变化，input_alive都要设置为1U，表示光标输入还活跃
        while (osMessageQueueGet(cursorHandle, &cur, NULL, 0U) == osOK)
        {
            if (cur.cursor_x != last_x || cur.cursor_y != last_y
             || cur.button_pressed != last_button)
            {
                changed = 1U;
            }
        }

        if ((HAL_GetTick() - last_time_tick) >= 1000U)
        {
            last_time_tick = HAL_GetTick();
            need_redraw = 1U;
        }
        //sel是当前选中的应用ID，如果光标移动到相邻应用的边界时，必须超过4个像素才会切换应用，否则保持原应用选中状态
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
            else if (key == '2' || key == '8' || key == '4' || key == '6')
            {
                int16_t dx = 0;
                int16_t dy = 0;
                int16_t step;
                uint8_t sens = Set_Sys_GetSensitivity();
                CursorMsg_t drop;

                if (sens == 0U) step = 3;
                else if (sens == 2U) step = 10;
                else step = 6;

                if (key == '2') dy = -step;
                if (key == '8') dy = step;
                if (key == '4') dx = -step;
                if (key == '6') dx = step;

                Cursor_KeyMove(dx, dy);
                Cursor_GetPos(&cur.cursor_x, &cur.cursor_y);

                while (osMessageQueueGet(cursorHandle, &drop, NULL, 0U) == osOK) { }

                need_redraw = 1U;
                sel = Desktop_SelectApp(&cur, sel);
            }
        }
        //光标按键按下时，只有当上一次按键状态为未按下时，才会触发应用切换，避免连续触发
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
            Desktop_Draw(&cur, sel);
            need_redraw = 0U;
        }

        last_x = cur.cursor_x;
        last_y = cur.cursor_y;
        last_button = cur.button_pressed;

        osDelay(10U);
    }
}
