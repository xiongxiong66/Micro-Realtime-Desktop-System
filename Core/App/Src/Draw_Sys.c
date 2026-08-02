/**
  ******************************************************************************
  * @file           : Draw_Sys.c
  * @brief          : Drawing app: picture selection and pixel editor
  ******************************************************************************
  */

#include "Draw_Sys.h"
#include "Oled_Sys.h"
#include "ImgFile.h"
#include "NameEdit_Sys.h"
#include "Confirm_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include "Sw_Adc_Sys.h"
#include <string.h>

#define DRAW_PAGE_ROWS     6U

static DrawFile_t draw_file;
static uint8_t draw_used[DRAW_SECTOR_COUNT];
static uint16_t draw_used_count;

static void draw_make_name(uint8_t idx, char *name)
{
    name[0] = 'I';
    name[1] = 'M';
    name[2] = 'G';
    name[3] = (char)('0' + (idx / 100U) % 10U);
    name[4] = (char)('0' + (idx / 10U) % 10U);
    name[5] = (char)('0' + idx % 10U);
    name[6] = '\0';
}

static void draw_scan(void)
{
    uint8_t hdr[16];

    draw_used_count = 0U;
    for (uint16_t i = 0; i < DRAW_SECTOR_COUNT; i++)
    {
        if (SFlash_Read((uint32_t)(DRAW_SECTOR_BASE + i) * SFLASH_SECTOR_SIZE, hdr, 16U) == SFLASH_OK
         && hdr[0] == DRAW_MAGIC0 && hdr[1] == DRAW_MAGIC1
         && hdr[2] == DRAW_MAGIC2 && hdr[3] == DRAW_MAGIC3)
        {
            draw_used[draw_used_count++] = (uint8_t)i;
        }
    }
}

static void draw_load_name(uint8_t idx, char *name)
{
    uint8_t hdr[16];
    uint8_t i;

    if (SFlash_Read((uint32_t)(DRAW_SECTOR_BASE + idx) * SFLASH_SECTOR_SIZE, hdr, 16U) != SFLASH_OK
     || hdr[0] != DRAW_MAGIC0 || hdr[1] != DRAW_MAGIC1
     || hdr[2] != DRAW_MAGIC2 || hdr[3] != DRAW_MAGIC3)
    {
        draw_make_name(idx, name);
        return;
    }

    for (i = 0; i < 11U; i++)
    {
        name[i] = (char)hdr[4U + i];
        if (name[i] == '\0') break;
    }
    name[i] = '\0';
}

static void draw_select_render(uint8_t page, uint8_t sel)
{
    uint8_t pages = (draw_used_count + DRAW_PAGE_ROWS - 1U) / DRAW_PAGE_ROWS;
    uint8_t used_on_page;
    char buf[16];

    if (pages == 0U) pages = 1U;

    /* Number of image rows on this page; NEW is always one row below them. */
    used_on_page = ((uint16_t)page * DRAW_PAGE_ROWS < draw_used_count)
                 ? (uint8_t)(draw_used_count - (uint16_t)page * DRAW_PAGE_ROWS)
                 : 0U;
    if (used_on_page > DRAW_PAGE_ROWS) used_on_page = DRAW_PAGE_ROWS;
    if (sel > used_on_page) sel = used_on_page;

    OLED_Clear();

    if (draw_used_count == 0U)
    {
        OLED_SetCursor(0, 0);
        OLED_PrintString("No Image");
    }
    else
    {
        for (uint8_t r = 0; r < DRAW_PAGE_ROWS; r++)
        {
            uint16_t n = (uint16_t)page * DRAW_PAGE_ROWS + r;
            if (n < draw_used_count)
            {
                draw_load_name(draw_used[n], buf);
                if (r == sel)
                {
                    OLED_SetCursor(0U, (uint8_t)(r * 8U));
                    OLED_PrintString(">>");
                    OLED_PrintString(buf);
                }
                else
                {
                    OLED_SetCursor(0U, (uint8_t)(r * 8U));
                    OLED_PrintString(buf);
                }
            }
        }
    }

    if (sel == used_on_page)
    {
        OLED_SetCursor(0U, 48U);
        OLED_PrintString(">>NEW");
    }
    else
    {
        OLED_SetCursor(0U, 48U);
        OLED_PrintString("NEW");
    }

    OLED_SetCursor(0, 56);
    OLED_PrintString("< ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);
    OLED_PrintString(" >");
    OLED_Display();
}

static void draw_toggle_pixel(int16_t x, int16_t y)
{
    uint16_t idx;

    if (x < 0 || x >= (int16_t)OLED_WIDTH) return;
    if (y < 0 || y >= (int16_t)OLED_HEIGHT) return;

    idx = (uint16_t)(y / 8) * OLED_WIDTH + (uint16_t)x;
    draw_file.data[idx] ^= (uint8_t)(1U << (y % 8));
}

static void draw_edit_render(int16_t cx, int16_t cy)
{
    int16_t x = cx, y = cy;

    if (x < 0) x = 0;
    if (x > (int16_t)(OLED_WIDTH - 2)) x = (int16_t)(OLED_WIDTH - 2);
    if (y < 0) y = 0;
    if (y > (int16_t)(OLED_HEIGHT - 2)) y = (int16_t)(OLED_HEIGHT - 2);

    OLED_Blit(draw_file.data);
    OLED_DrawPixel((uint8_t)x, (uint8_t)y, OLED_WHITE);
    OLED_DrawPixel((uint8_t)(x + 1), (uint8_t)y, OLED_WHITE);
    OLED_DrawPixel((uint8_t)x, (uint8_t)(y + 1), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(x + 1), (uint8_t)(y + 1), OLED_WHITE);
    OLED_Display();
}

static void draw_save(uint8_t idx)
{
    if (SFlash_SaveSector(DRAW_SECTOR_BASE + idx, (const uint8_t *)&draw_file, sizeof(DrawFile_t)) == SFLASH_OK)
    {
        OLED_Clear();
        OLED_SetCursor(20, 28);
        OLED_PrintString("SAVED");
        OLED_Display();
        osDelay(300U);
    }
}

static void draw_edit(uint8_t idx, uint8_t is_new)
{
    int16_t cx = 64, cy = 32;
    int16_t joy_last_x = 0, joy_last_y = 0;
    uint8_t prev_button = 1U;
    uint8_t joy_synced = 0U;
    uint8_t keypad_used = 0U;
    char key;

    for (;;)
    {
        CursorMsg_t msg;

        while (osMessageQueueGet(cursorHandle, &msg, NULL, 0U) == osOK)
        {
            if (!joy_synced)
            {
                /* Baseline the joystick; keep the keypad position if it moved first. */
                if (!keypad_used)
                {
                    cx = msg.cursor_x;
                    cy = msg.cursor_y;
                }
                joy_last_x = msg.cursor_x;
                joy_last_y = msg.cursor_y;
                joy_synced = 1U;
            }
            else
            {
                /* Apply joystick movement as a delta so keypad moves are not overwritten. */
                cx += (int16_t)(msg.cursor_x - joy_last_x);
                cy += (int16_t)(msg.cursor_y - joy_last_y);
                joy_last_x = msg.cursor_x;
                joy_last_y = msg.cursor_y;

                if (cx < 0) cx = 0;
                if (cx > (int16_t)(OLED_WIDTH - 1)) cx = (int16_t)(OLED_WIDTH - 1);
                if (cy < 0) cy = 0;
                if (cy > (int16_t)(OLED_HEIGHT - 1)) cy = (int16_t)(OLED_HEIGHT - 1);
            }

            if (msg.button_pressed != 0U && prev_button == 0U)
            {
                draw_toggle_pixel(cx, cy);
            }
            prev_button = (msg.button_pressed != 0U) ? 1U : 0U;
        }

        while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK)
        {
            switch (key)
            {
                case '2':
                    keypad_used = 1U;
                    cy = (cy > 1) ? (int16_t)(cy - 1) : 0;
                    break;
                case '8':
                    keypad_used = 1U;
                    cy = (cy + 1 < (int16_t)OLED_HEIGHT) ? (int16_t)(cy + 1) : (int16_t)(OLED_HEIGHT - 1);
                    break;
                case '4':
                    keypad_used = 1U;
                    cx = (cx > 1) ? (int16_t)(cx - 1) : 0;
                    break;
                case '6':
                    keypad_used = 1U;
                    cx = (cx + 1 < (int16_t)OLED_WIDTH) ? (int16_t)(cx + 1) : (int16_t)(OLED_WIDTH - 1);
                    break;
                case '5':
                    draw_toggle_pixel(cx, cy);
                    break;
                case '0':
                    memset(draw_file.data, 0, OLED_BUFFER_SIZE);
                    break;
                case '#':
                    if (NameEdit_Run(draw_file.name, (uint8_t)sizeof(draw_file.name), !is_new))
                    {
                        draw_save(idx);
                        return;
                    }
                    break;
                case '*':
                    return;
                default:
                    break;
            }
        }

        draw_edit_render(cx, cy);
        osDelay(10U);
    }
}

static void draw_new(void)
{
    uint8_t idx = 0xFFU;
    uint16_t i, k;

    for (i = 0; i < DRAW_SECTOR_COUNT; i++)
    {
        uint8_t found = 0U;
        for (k = 0; k < draw_used_count; k++)
        {
            if (draw_used[k] == (uint8_t)i)
            {
                found = 1U;
                break;
            }
        }
        if (!found)
        {
            idx = (uint8_t)i;
            break;
        }
    }

    if (idx == 0xFFU) return;

    memset(&draw_file, 0, sizeof(draw_file));
    draw_file.magic[0] = DRAW_MAGIC0;
    draw_file.magic[1] = DRAW_MAGIC1;
    draw_file.magic[2] = DRAW_MAGIC2;
    draw_file.magic[3] = DRAW_MAGIC3;
    draw_make_name(idx, draw_file.name);

    draw_edit(idx, 1U);
}

static void draw_selection(void)
{
    uint8_t page = 0U;
    uint8_t sel = 0U;
    char key;

    for (;;)
    {
        uint8_t pages;
        uint8_t used_on_page;
        uint16_t n;

        draw_scan();

        pages = (draw_used_count + DRAW_PAGE_ROWS - 1U) / DRAW_PAGE_ROWS;
        if (pages == 0U) pages = 1U;
        if (page >= pages) page = (uint8_t)(pages - 1U);

        n = (uint16_t)page * DRAW_PAGE_ROWS;
        used_on_page = (n < draw_used_count)
                     ? (uint8_t)(draw_used_count - n)
                     : 0U;
        if (used_on_page > DRAW_PAGE_ROWS) used_on_page = DRAW_PAGE_ROWS;
        if (sel > used_on_page) sel = used_on_page;

        draw_select_render(page, sel);

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        /* Selection: 2/8 move, 4/6 page, # opens the selected entry. */
        switch (key)
        {
            case '2':
                sel = (sel > 0U) ? (uint8_t)(sel - 1U) : used_on_page;
                break;
            case '8':
                sel = (sel < used_on_page) ? (uint8_t)(sel + 1U) : 0U;
                break;
            case '4':
                if (page > 0U)
                {
                    page--;
                    sel = 0U;
                }
                break;
            case '6':
                if (page + 1U < pages)
                {
                    page++;
                    sel = 0U;
                }
                break;
            case '#':
                if (sel < used_on_page)
                {
                    uint8_t idx = draw_used[n + sel];
                    if (SFlash_LoadSector(DRAW_SECTOR_BASE + idx, (uint8_t *)&draw_file, sizeof(DrawFile_t)) == SFLASH_OK)
                    {
                        draw_edit(idx, 0U);
                    }
                }
                else
                {
                    draw_new();
                }
                break;
            case 'D':
                if (sel < used_on_page)
                {
                    uint8_t idx = draw_used[n + sel];
                    char name[16];

                    draw_load_name(idx, name);
                    if (Confirm_Delete(name))
                    {
                        SFlash_EraseSector(DRAW_SECTOR_BASE + idx);
                    }
                }
                break;
            case '*':
                return;
            default:
                break;
        }
    }
}

void App_Draw_Task_Sys(void)
{
    /* 上电后先自挂起，等待桌面任务恢复 */
    osThreadSuspend(osThreadGetId());

    for (;;)
    {
        draw_selection();

        /* 从画图应用退出：唤醒桌面，自身回到挂起态 */
        osThreadResume(oledHandle);
        osThreadSuspend(osThreadGetId());
    }
}
