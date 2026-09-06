/**
  ******************************************************************************
  * @file           : CursorView.c
  * @brief          : Shared OLED cursor overlay using XOR pixel toggling
  ******************************************************************************
  */

#include "CursorView.h"
#include "Sw_Adc_App.h"
#include "Set_App.h"
#include "oled.h"

static int16_t cursor_view_x = -1;
static int16_t cursor_view_y = -1;
static uint8_t cursor_view_size = 0U;

static void CursorView_Clamp(int16_t *x, int16_t *y, uint8_t cs)
{
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x > (int16_t)(OLED_WIDTH - cs)) *x = (int16_t)(OLED_WIDTH - cs);
    if (*y > (int16_t)(OLED_HEIGHT - cs)) *y = (int16_t)(OLED_HEIGHT - cs);
}

static void CursorView_ToggleBox(int16_t x, int16_t y, uint8_t cs)
{
    int16_t s = (int16_t)cs;

    for (int16_t yy = 0; yy < s; yy++)
    {
        for (int16_t xx = 0; xx < s; xx++)
        {
            OLED_TogglePixel((uint8_t)(x + xx), (uint8_t)(y + yy));
        }
    }
}

static void CursorView_UploadBox(int16_t x, int16_t y, uint8_t cs)
{
    OLED_UpdateRect((uint8_t)x, (uint8_t)y, cs, cs);
}

void CursorView_Place(void)
{
    int16_t x;
    int16_t y;
    uint8_t cs = Set_Sys_GetCursorPixels();

    Cursor_GetPos(&x, &y);
    CursorView_Clamp(&x, &y, cs);

    CursorView_ToggleBox(x, y, cs);
    CursorView_UploadBox(x, y, cs);

    cursor_view_x = x;
    cursor_view_y = y;
    cursor_view_size = cs;
}

void CursorView_Erase(void)
{
    if (cursor_view_size != 0U && cursor_view_x >= 0)
    {
        CursorView_ToggleBox(cursor_view_x, cursor_view_y, cursor_view_size);
        CursorView_UploadBox(cursor_view_x, cursor_view_y, cursor_view_size);
    }

    cursor_view_x = -1;
    cursor_view_y = -1;
    cursor_view_size = 0U;
}

void CursorView_Track(void)
{
    int16_t x;
    int16_t y;
    uint8_t cs = Set_Sys_GetCursorPixels();

    Cursor_GetPos(&x, &y);
    CursorView_Clamp(&x, &y, cs);

    if (cursor_view_size == cs
     && cursor_view_x == x
     && cursor_view_y == y)
    {
        return;
    }

    if (cursor_view_size != 0U && cursor_view_x >= 0)
    {
        CursorView_ToggleBox(cursor_view_x, cursor_view_y, cursor_view_size);
        CursorView_UploadBox(cursor_view_x, cursor_view_y, cursor_view_size);
    }

    CursorView_ToggleBox(x, y, cs);
    CursorView_UploadBox(x, y, cs);

    cursor_view_x = x;
    cursor_view_y = y;
    cursor_view_size = cs;
}
