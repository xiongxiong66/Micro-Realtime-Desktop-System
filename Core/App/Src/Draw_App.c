/**
  ******************************************************************************
  * @file           : Draw_App.c
  * @brief          : Drawing app: picture selection and pixel editor
  ******************************************************************************
  */

#include "Draw_App.h"
#include "TaskWatch.h"
#include "Oled_App.h"
#include "ImgFile.h"
#include "Music_App.h"
#include "Set_App.h"
#include "Log_App.h"
#include "FileTab.h"
#include "NameEdit_App.h"
#include "Confirm_App.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include "Sw_Adc_App.h"
#include <string.h>

#define DRAW_PAGE_ROWS     6U       //每页显示的图片行数
#define DRAW_PEN_DOT          0U    //画点模式
#define DRAW_PEN_LINE         1U    //画线模式
#define DRAW_PEN_RECT         2U    //画空心矩形
#define DRAW_PEN_ERASE        3U    //矩形区域擦除
#define DRAW_PEN_CIRCLE       4U    //画空心圆
#define DRAW_PEN_FILL_CIRCLE  5U    //画实心圆
#define DRAW_PEN_INVERT       6U    //整图背景/笔迹反色

static const uint8_t draw_cycle[] = {
    DRAW_PEN_DOT,
    DRAW_PEN_LINE,
    DRAW_PEN_RECT,
    DRAW_PEN_CIRCLE,
    DRAW_PEN_FILL_CIRCLE,
    DRAW_PEN_INVERT
};
#define DRAW_CYCLE_COUNT  (uint8_t)(sizeof(draw_cycle) / sizeof(draw_cycle[0]))

static DrawFile_t draw_file;
static uint8_t draw_used[DRAW_SECTOR_COUNT];
static uint16_t draw_used_count;
static uint8_t draw_pen_mode = DRAW_PEN_DOT;
static uint8_t draw_anchor_pending = 0U;
static int16_t draw_anchor_x = 0;
static int16_t draw_anchor_y = 0;
//@brief:根据索引生成默认图片名称，格式为"IMGxxx"，xxx为三位数字
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
//@brief:扫描所有图片存储扇区，只认魔数，打开文件时才做完整校验
static void draw_scan(void)
{
    draw_used_count = 0U;
    for (uint16_t i = 0U; i < DRAW_SECTOR_COUNT; i++)
    {
        if (DrawFile_MagicValid((uint16_t)(DRAW_SECTOR_BASE + i)))
        {
            draw_used[draw_used_count++] = (uint8_t)i;
        }
        if ((i & 0x1FU) == 0x1FU)
        {
            TaskWatch_Beat(TASKWATCH_OLED);
        }
    }
    TaskWatch_Beat(TASKWATCH_OLED);
}
//@brief:根据索引加载图片文件到draw_file结构体中，如果索引无效或读取失败，则将默认名称写入name，如果有效，则将图片名称写入name
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
        //hdr前四个字节为图片标志，接下来的12个字节为图片名称
        name[i] = (char)hdr[4U + i];
        if (name[i] == '\0') break;
    }
    name[i] = '\0';
}
//
static void draw_select_render(uint8_t page, uint8_t sel)
{
    //计算总页数，确保至少有一页
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

static void draw_set_pixel(int16_t x, int16_t y)
{
    uint16_t idx;

    if (x < 0 || x >= (int16_t)OLED_WIDTH) return;
    if (y < 0 || y >= (int16_t)OLED_HEIGHT) return;

    idx = (uint16_t)(y / 8) * OLED_WIDTH + (uint16_t)x;
    draw_file.data[idx] |= (uint8_t)(1U << (y % 8));
}

static void draw_clear_pixel(int16_t x, int16_t y)
{
    uint16_t idx;

    if (x < 0 || x >= (int16_t)OLED_WIDTH) return;
    if (y < 0 || y >= (int16_t)OLED_HEIGHT) return;

    idx = (uint16_t)(y / 8) * OLED_WIDTH + (uint16_t)x;
    draw_file.data[idx] &= (uint8_t)~(1U << (y % 8));
}

static void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t dy = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (int16_t)(dx - dy);

    for (;;)
    {
        draw_set_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = (int16_t)(err * 2);
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_rect_outline(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t xa = (x0 < x1) ? x0 : x1;
    int16_t xb = (x0 < x1) ? x1 : x0;
    int16_t ya = (y0 < y1) ? y0 : y1;
    int16_t yb = (y0 < y1) ? y1 : y0;

    draw_line(xa, ya, xb, ya);
    draw_line(xb, ya, xb, yb);
    draw_line(xb, yb, xa, yb);
    draw_line(xa, yb, xa, ya);
}

static void draw_circle_plot_data(int16_t cx, int16_t cy, int16_t x, int16_t y)
{
    draw_set_pixel((int16_t)(cx + x), (int16_t)(cy + y));
    draw_set_pixel((int16_t)(cx + x), (int16_t)(cy - y));
    draw_set_pixel((int16_t)(cx - x), (int16_t)(cy + y));
    draw_set_pixel((int16_t)(cx - x), (int16_t)(cy - y));
    draw_set_pixel((int16_t)(cx + y), (int16_t)(cy + x));
    draw_set_pixel((int16_t)(cx + y), (int16_t)(cy - x));
    draw_set_pixel((int16_t)(cx - y), (int16_t)(cy + x));
    draw_set_pixel((int16_t)(cx - y), (int16_t)(cy - x));
}

static void draw_circle_outline_data(int16_t cx, int16_t cy, int16_t r)
{
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    if (r < 0) return;

    while (x >= y)
    {
        draw_circle_plot_data(cx, cy, x, y);
        y++;
        if (err <= 0)
        {
            err = (int16_t)(err + 2 * y + 1);
        }
        if (err > 0)
        {
            x--;
            err = (int16_t)(err - 2 * x - 1);
        }
    }
}

static void draw_filled_circle_data(int16_t cx, int16_t cy, int16_t r)
{
    int16_t x, y;

    if (r < 0) return;

    for (y = (int16_t)(cy - r); y <= (int16_t)(cy + r); y++)
    {
        for (x = (int16_t)(cx - r); x <= (int16_t)(cx + r); x++)
        {
            int16_t dx = (x >= cx) ? (int16_t)(x - cx) : (int16_t)(cx - x);
            int16_t dy = (y >= cy) ? (int16_t)(y - cy) : (int16_t)(cy - y);

            if (dx * dx + dy * dy <= r * r)
            {
                draw_set_pixel(x, y);
            }
        }
    }
}

static int16_t draw_anchor_radius(int16_t cx, int16_t cy, int16_t x, int16_t y)
{
    int16_t dx = (x > cx) ? (int16_t)(x - cx) : (int16_t)(cx - x);
    int16_t dy = (y > cy) ? (int16_t)(y - cy) : (int16_t)(cy - y);

    return (dx > dy) ? dx : dy;
}

static void draw_invert_image(void)
{
    uint16_t i;

    for (i = 0U; i < OLED_BUFFER_SIZE; i++)
    {
        draw_file.data[i] ^= 0xFFU;
    }
}

static void draw_preview_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t dy = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (int16_t)(dx - dy);

    for (;;)
    {
        OLED_DrawPixel((uint8_t)x0, (uint8_t)y0, OLED_WHITE);
        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = (int16_t)(err * 2);
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_erase_region(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t xa = (x0 < x1) ? x0 : x1;
    int16_t xb = (x0 < x1) ? x1 : x0;
    int16_t ya = (y0 < y1) ? y0 : y1;
    int16_t yb = (y0 < y1) ? y1 : y0;

    for (int16_t y = ya; y <= yb; y++)
    {
        for (int16_t x = xa; x <= xb; x++)
        {
            draw_clear_pixel(x, y);
        }
    }
}

static void draw_preview_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t xa = (x0 < x1) ? x0 : x1;
    int16_t xb = (x0 < x1) ? x1 : x0;
    int16_t ya = (y0 < y1) ? y0 : y1;
    int16_t yb = (y0 < y1) ? y1 : y0;

    for (int16_t x = xa; x <= xb; x++)
    {
        OLED_DrawPixel((uint8_t)x, (uint8_t)ya, OLED_WHITE);
        OLED_DrawPixel((uint8_t)x, (uint8_t)yb, OLED_WHITE);
    }
    for (int16_t y = ya; y <= yb; y++)
    {
        OLED_DrawPixel((uint8_t)xa, (uint8_t)y, OLED_WHITE);
        OLED_DrawPixel((uint8_t)xb, (uint8_t)y, OLED_WHITE);
    }
}

static void draw_circle_plot_preview(int16_t cx, int16_t cy, int16_t x, int16_t y)
{
    OLED_DrawPixel((uint8_t)(cx + x), (uint8_t)(cy + y), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx + x), (uint8_t)(cy - y), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx - x), (uint8_t)(cy + y), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx - x), (uint8_t)(cy - y), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx + y), (uint8_t)(cy + x), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx + y), (uint8_t)(cy - x), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx - y), (uint8_t)(cy + x), OLED_WHITE);
    OLED_DrawPixel((uint8_t)(cx - y), (uint8_t)(cy - x), OLED_WHITE);
}

static void draw_circle_outline_preview(int16_t cx, int16_t cy, int16_t r)
{
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    if (r < 0) return;

    while (x >= y)
    {
        draw_circle_plot_preview(cx, cy, x, y);
        y++;
        if (err <= 0)
        {
            err = (int16_t)(err + 2 * y + 1);
        }
        if (err > 0)
        {
            x--;
            err = (int16_t)(err - 2 * x - 1);
        }
    }
}

static void draw_filled_circle_preview(int16_t cx, int16_t cy, int16_t r)
{
    int16_t x, y;

    if (r < 0) return;

    for (y = (int16_t)(cy - r); y <= (int16_t)(cy + r); y++)
    {
        for (x = (int16_t)(cx - r); x <= (int16_t)(cx + r); x++)
        {
            int16_t dx = (x >= cx) ? (int16_t)(x - cx) : (int16_t)(cx - x);
            int16_t dy = (y >= cy) ? (int16_t)(y - cy) : (int16_t)(cy - y);

            if (dx * dx + dy * dy <= r * r)
            {
                OLED_DrawPixel((uint8_t)x, (uint8_t)y, OLED_WHITE);
            }
        }
    }
}

static void draw_cursor_box(int16_t x, int16_t y, int16_t s, uint8_t mark_center)
{
    int16_t i;

    for (i = 0; i < s; i++)
    {
        OLED_DrawPixel((uint8_t)(x + i), (uint8_t)y, OLED_WHITE);
        OLED_DrawPixel((uint8_t)(x + i), (uint8_t)(y + s - 1), OLED_WHITE);
        OLED_DrawPixel((uint8_t)x, (uint8_t)(y + i), OLED_WHITE);
        OLED_DrawPixel((uint8_t)(x + s - 1), (uint8_t)(y + i), OLED_WHITE);
    }
    if (mark_center != 0U && s >= 3)
    {
        OLED_DrawPixel((uint8_t)(x + s / 2), (uint8_t)(y + s / 2), OLED_WHITE);
    }
}

static void draw_cursor_invert(int16_t x, int16_t y, int16_t s)
{
    int16_t i;

    draw_cursor_box(x, y, s, 0U);
    for (i = 1; i < s - 1; i++)
    {
        OLED_DrawPixel((uint8_t)(x + i), (uint8_t)(y + i), OLED_WHITE);
        OLED_DrawPixel((uint8_t)(x + s - 1 - i), (uint8_t)(y + i), OLED_WHITE);
    }
}

static void draw_pen_action(int16_t cx, int16_t cy)
{
    if (draw_pen_mode == DRAW_PEN_DOT)
    {
        draw_toggle_pixel(cx, cy);
        return;
    }
    if (draw_pen_mode == DRAW_PEN_INVERT)
    {
        draw_invert_image();
        return;
    }

    if (!draw_anchor_pending)
    {
        draw_anchor_x = cx;
        draw_anchor_y = cy;
        draw_anchor_pending = 1U;
    }
    else
    {
        int16_t r;

        switch (draw_pen_mode)
        {
            case DRAW_PEN_LINE:
                draw_line(draw_anchor_x, draw_anchor_y, cx, cy);
                break;
            case DRAW_PEN_RECT:
                draw_rect_outline(draw_anchor_x, draw_anchor_y, cx, cy);
                break;
            case DRAW_PEN_ERASE:
                draw_erase_region(draw_anchor_x, draw_anchor_y, cx, cy);
                break;
            case DRAW_PEN_CIRCLE:
            case DRAW_PEN_FILL_CIRCLE:
                r = draw_anchor_radius(draw_anchor_x, draw_anchor_y, cx, cy);
                if (draw_pen_mode == DRAW_PEN_FILL_CIRCLE)
                {
                    draw_filled_circle_data(draw_anchor_x, draw_anchor_y, r);
                }
                else
                {
                    draw_circle_outline_data(draw_anchor_x, draw_anchor_y, r);
                }
                break;
            default:
                break;
        }
        draw_anchor_pending = 0U;
    }
}

static const char *draw_mode_text(void)
{
    switch (draw_pen_mode)
    {
        case DRAW_PEN_LINE:
            return "LINE";
        case DRAW_PEN_RECT:
            return "RECT";
        case DRAW_PEN_ERASE:
            return "ERASE";
        case DRAW_PEN_CIRCLE:
            return "CIRC";
        case DRAW_PEN_FILL_CIRCLE:
            return "FILL";
        case DRAW_PEN_INVERT:
            return "INV ";
        default:
            return "DOT ";
    }
}

static void draw_edit_render(int16_t cx, int16_t cy)
{
    int16_t x = cx, y = cy;
    uint8_t cs = Set_Sys_GetCursorPixels();
    int16_t s;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (draw_pen_mode == DRAW_PEN_DOT)
    {
        if (x > (int16_t)(OLED_WIDTH - cs)) x = (int16_t)(OLED_WIDTH - cs);
        if (y > (int16_t)(OLED_HEIGHT - cs)) y = (int16_t)(OLED_HEIGHT - cs);
    }
    else
    {
        if (x > (int16_t)(OLED_WIDTH - 1)) x = (int16_t)(OLED_WIDTH - 1);
        if (y > (int16_t)(OLED_HEIGHT - 1)) y = (int16_t)(OLED_HEIGHT - 1);
    }

    OLED_Blit(draw_file.data);
    OLED_FillRect(0, 56, OLED_WIDTH, 8U, OLED_BLACK);

    if (draw_anchor_pending)
    {
        if (draw_pen_mode == DRAW_PEN_LINE)
        {
            draw_preview_line(draw_anchor_x, draw_anchor_y, x, y);
            OLED_DrawPixel((uint8_t)draw_anchor_x, (uint8_t)draw_anchor_y, OLED_WHITE);
        }
        else if (draw_pen_mode == DRAW_PEN_RECT
              || draw_pen_mode == DRAW_PEN_ERASE)
        {
            draw_preview_rect(draw_anchor_x, draw_anchor_y, x, y);
        }
        else if (draw_pen_mode == DRAW_PEN_CIRCLE
              || draw_pen_mode == DRAW_PEN_FILL_CIRCLE)
        {
            int16_t r = draw_anchor_radius(draw_anchor_x, draw_anchor_y, x, y);
            draw_circle_outline_preview(draw_anchor_x, draw_anchor_y, r);
        }
    }

    if (draw_pen_mode == DRAW_PEN_LINE)
    {
        /* 画线模式：十字准星光标 */
        int16_t arm = (cs > 1U) ? (int16_t)cs : 1;

        for (int16_t i = -arm; i <= arm; i++)
        {
            if (x + i >= 0 && x + i < (int16_t)OLED_WIDTH)
                OLED_DrawPixel((uint8_t)(x + i), (uint8_t)y, OLED_WHITE);
            if (y + i >= 0 && y + i < (int16_t)OLED_HEIGHT)
                OLED_DrawPixel((uint8_t)x, (uint8_t)(y + i), OLED_WHITE);
        }
    }
    else if (draw_pen_mode == DRAW_PEN_RECT)
    {
        /* 画矩形模式：空心方框加中心点，与擦除图标区分 */
        s = (cs > 1U) ? (int16_t)cs : 3;
        draw_cursor_box(x, y, s, 1U);
    }
    else if (draw_pen_mode == DRAW_PEN_ERASE)
    {
        /* 区域擦除模式：空心方框光标 */
        s = (cs > 1U) ? (int16_t)cs : 3;
        draw_cursor_box(x, y, s, 0U);
    }
    else if (draw_pen_mode == DRAW_PEN_CIRCLE)
    {
        s = (int16_t)((cs + 1U) / 2U);
        draw_circle_outline_preview(x, y, s);
    }
    else if (draw_pen_mode == DRAW_PEN_FILL_CIRCLE)
    {
        s = (int16_t)((cs + 1U) / 2U);
        draw_filled_circle_preview(x, y, s);
    }
    else if (draw_pen_mode == DRAW_PEN_DOT)
    {
        for (uint8_t yy = 0U; yy < cs; yy++)
        {
            for (uint8_t xx = 0U; xx < cs; xx++)
            {
                OLED_DrawPixel((uint8_t)(x + xx), (uint8_t)(y + yy), OLED_WHITE);
            }
        }
    }
    else
    {
        s = (cs > 1U) ? (int16_t)cs : 3;
        draw_cursor_invert(x, y, s);
    }

    OLED_PrintStringColor(0, 56, draw_mode_text(), OLED_WHITE);
    OLED_Display();
}

static void draw_save(uint8_t idx, uint8_t is_new)
{
    draw_file.checksum = DrawFile_ComputeChecksum(&draw_file);

    if (SFlash_SaveSector(DRAW_SECTOR_BASE + idx, (const uint8_t *)&draw_file, sizeof(DrawFile_t)) == SFLASH_OK)
    {
        if (is_new != 0U)
        {
            (void)FileTab_AdjustCount(FILE_TAB_KIND_PIC, 1, TASKWATCH_OLED);
        }
        OLED_Clear();
        OLED_SetCursor(20, 28);
        OLED_PrintString("SAVED");
        OLED_Display();
        osDelay(300U);
    }
}

static void draw_next_mode(void)
{
    uint8_t i;

    for (i = 0U; i < DRAW_CYCLE_COUNT; i++)
    {
        if (draw_cycle[i] == draw_pen_mode)
        {
            draw_pen_mode = draw_cycle[(uint8_t)((i + 1U) % DRAW_CYCLE_COUNT)];
            draw_anchor_pending = 0U;
            return;
        }
    }

    /* 矩形擦除不在 C 循环内，按 C 时从擦除回到画点 */
    draw_pen_mode = DRAW_PEN_DOT;
    draw_anchor_pending = 0U;
}

static void draw_edit(uint8_t idx, uint8_t is_new)
{
    int16_t cx = 64, cy = 32;
    int16_t joy_last_x = 0, joy_last_y = 0;
    uint8_t prev_button = 1U;
    uint8_t joy_synced = 0U;
    uint8_t keypad_used = 0U;
    char key;

    draw_pen_mode = DRAW_PEN_DOT;
    draw_anchor_pending = 0U;

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_OLED);

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
                draw_pen_action(cx, cy);
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
                    draw_pen_action(cx, cy);
                    break;
                case '0':
                    memset(draw_file.data, 0, OLED_BUFFER_SIZE);
                    draw_anchor_pending = 0U;
                    break;
                case 'C':
                    draw_next_mode();
                    break;
                case 'D':
                    draw_pen_mode = DRAW_PEN_ERASE;
                    draw_anchor_pending = 0U;
                    break;
                case '#':
                    {
                        char old_name[sizeof(draw_file.name)];

                        memcpy(old_name, draw_file.name, sizeof(old_name));
                        if (NameEdit_Run(draw_file.name, (uint8_t)sizeof(draw_file.name), !is_new, Draw_NameUsed))
                        {
                            draw_save(idx, is_new);
                            if (is_new)
                            {
                                Log_Write(LOG_TYPE_DRAW, "NEW");
                            }
                            else if (strcmp(old_name, draw_file.name) != 0)
                            {
                                Log_Write(LOG_TYPE_DRAW, "RENAMED");
                            }
                            else
                            {
                                Log_Write(LOG_TYPE_DRAW, "MODIFIED");
                            }
                            return;
                        }
                    }
                    break;
                case '1':
                    Music_Bg_Toggle();
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

void Draw_App_New(void)
{
    draw_scan();
    draw_new();
}

static void draw_selection(void)
{
    uint8_t page = 0U;
    uint8_t sel = 0U;
    uint8_t refresh = 1U;
    char key;

    for (;;)
    {
        uint8_t pages;
        uint8_t used_on_page;
        uint16_t n;

        TaskWatch_Beat(TASKWATCH_OLED);
        if (refresh != 0U)
        {
            draw_scan();
            refresh = 0U;
        }

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
                    if (DrawFile_LoadValid((uint16_t)(DRAW_SECTOR_BASE + idx), &draw_file))
                    {
                        draw_edit(idx, 0U);
                    }
                    else
                    {
                        OLED_Clear();
                        OLED_SetCursor(16, 28);
                        OLED_PrintString("INVALID");
                        OLED_Display();
                        osDelay(300U);
                    }
                }
                else
                {
                    draw_new();
                }
                refresh = 1U;
                break;
            case 'D':
                if (sel < used_on_page)
                {
                    uint8_t idx = draw_used[n + sel];
                    char name[16];

                    draw_load_name(idx, name);
                    if (Confirm_Delete(name))
                    {
                        if (SFlash_EraseSector(DRAW_SECTOR_BASE + idx) == SFLASH_OK)
                        {
                            (void)FileTab_AdjustCount(FILE_TAB_KIND_PIC, -1,
                                                      TASKWATCH_OLED);
                            Log_Write(LOG_TYPE_DRAW, "DELETED");
                        }
                        refresh = 1U;
                    }
                }
                break;
            case '1':
                Music_Bg_Toggle();
                break;
            case '*':
                return;
            default:
                break;
        }
    }
}

void Draw_Sys_Run(void)
{
    draw_selection();
}

uint8_t Draw_NameUsed(const char *new_name, const char *old_name)
{
    uint8_t hdr[16];
    char other[12];
    uint16_t i;

    if (new_name == NULL || new_name[0] == '\0') return 0U;

    for (i = 0U; i < DRAW_SECTOR_COUNT; i++)
    {
        if (SFlash_Read((uint32_t)(DRAW_SECTOR_BASE + i) * SFLASH_SECTOR_SIZE,
                        hdr, 16U) != SFLASH_OK) continue;
        if (hdr[0] != DRAW_MAGIC0 || hdr[1] != DRAW_MAGIC1
         || hdr[2] != DRAW_MAGIC2 || hdr[3] != DRAW_MAGIC3) continue;

        for (uint8_t j = 0U; j < 11U; j++)
        {
            other[j] = (char)hdr[4U + j];
            if (other[j] == '\0') break;
        }
        other[11] = '\0';

        if (strcmp(other, new_name) == 0
         && (old_name == NULL || strcmp(other, old_name) != 0))
        {
            return 1U;
        }
    }
    return 0U;
}
