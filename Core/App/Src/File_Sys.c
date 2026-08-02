/**
  ******************************************************************************
  * @file           : File_Sys.c
  * @brief          : File manager app: music and picture lists
  ******************************************************************************
  */

#include "File_Sys.h"
#include "Oled_Sys.h"
#include "ImgFile.h"
#include "NameEdit_Sys.h"
#include "Confirm_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include <string.h>

#define FILE_PAGE_ROWS     6U

/* Music region, see README W25Q64 partition table */
#define FILE_MUSIC_BASE    544U
#define FILE_MUSIC_COUNT   256U

#define FILE_MUSIC_MAGIC0  'M'
#define FILE_MUSIC_MAGIC1  'U'
#define FILE_MUSIC_MAGIC2  'S'
#define FILE_MUSIC_MAGIC3  '1'

typedef struct {
    uint8_t magic[4];
    char name[12];
    uint16_t size;
} MusicFile_t;

static DrawFile_t file_draw;
static MusicFile_t file_music;
static uint8_t file_pic_idx[DRAW_SECTOR_COUNT];
static uint16_t file_pic_count;
static uint8_t file_mus_idx[FILE_MUSIC_COUNT];
static uint16_t file_mus_count;

static void file_load_pic_name(uint8_t idx, char *name)
{
    uint8_t hdr[16];
    uint8_t i;

    if (SFlash_Read((uint32_t)(DRAW_SECTOR_BASE + idx) * SFLASH_SECTOR_SIZE, hdr, 16U) != SFLASH_OK
     || hdr[0] != DRAW_MAGIC0 || hdr[1] != DRAW_MAGIC1
     || hdr[2] != DRAW_MAGIC2 || hdr[3] != DRAW_MAGIC3)
    {
        name[0] = '\0';
        return;
    }

    for (i = 0; i < 11U; i++)
    {
        name[i] = (char)hdr[4U + i];
        if (name[i] == '\0') break;
    }
    name[i] = '\0';
}

static void file_load_music_name(uint8_t idx, char *name)
{
    uint8_t hdr[18];
    uint8_t i;

    if (SFlash_Read((uint32_t)(FILE_MUSIC_BASE + idx) * SFLASH_SECTOR_SIZE, hdr, 18U) != SFLASH_OK
     || hdr[0] != FILE_MUSIC_MAGIC0 || hdr[1] != FILE_MUSIC_MAGIC1
     || hdr[2] != FILE_MUSIC_MAGIC2 || hdr[3] != FILE_MUSIC_MAGIC3)
    {
        name[0] = '\0';
        return;
    }

    for (i = 0; i < 11U; i++)
    {
        name[i] = (char)hdr[4U + i];
        if (name[i] == '\0') break;
    }
    name[i] = '\0';
}

static void file_scan_pictures(void)
{
    uint8_t hdr[16];

    file_pic_count = 0U;
    for (uint16_t i = 0; i < DRAW_SECTOR_COUNT; i++)
    {
        if (SFlash_Read((uint32_t)(DRAW_SECTOR_BASE + i) * SFLASH_SECTOR_SIZE, hdr, 16U) == SFLASH_OK
         && hdr[0] == DRAW_MAGIC0 && hdr[1] == DRAW_MAGIC1
         && hdr[2] == DRAW_MAGIC2 && hdr[3] == DRAW_MAGIC3)
        {
            file_pic_idx[file_pic_count++] = (uint8_t)i;
        }
    }
}

static void file_scan_music(void)
{
    uint8_t hdr[18];

    file_mus_count = 0U;
    for (uint16_t i = 0; i < FILE_MUSIC_COUNT; i++)
    {
        if (SFlash_Read((uint32_t)(FILE_MUSIC_BASE + i) * SFLASH_SECTOR_SIZE, hdr, 18U) == SFLASH_OK
         && hdr[0] == FILE_MUSIC_MAGIC0 && hdr[1] == FILE_MUSIC_MAGIC1
         && hdr[2] == FILE_MUSIC_MAGIC2 && hdr[3] == FILE_MUSIC_MAGIC3)
        {
            file_mus_idx[file_mus_count++] = (uint8_t)i;
        }
    }
}

static void file_pic_render(uint8_t page, uint8_t sel)
{
    uint8_t pages = (file_pic_count + FILE_PAGE_ROWS - 1U) / FILE_PAGE_ROWS;
    uint8_t used_on_page;
    char name[16];

    if (pages == 0U) pages = 1U;

    used_on_page = ((uint16_t)page * FILE_PAGE_ROWS < file_pic_count)
                 ? (uint8_t)(file_pic_count - (uint16_t)page * FILE_PAGE_ROWS)
                 : 0U;
    if (used_on_page > FILE_PAGE_ROWS) used_on_page = FILE_PAGE_ROWS;
    if (sel > used_on_page) sel = used_on_page;

    OLED_Clear();

    if (file_pic_count == 0U)
    {
        OLED_SetCursor(0, 8);
        OLED_PrintString("No Image");
    }
    else
    {
        for (uint8_t r = 0; r < FILE_PAGE_ROWS; r++)
        {
            uint16_t n = (uint16_t)page * FILE_PAGE_ROWS + r;
            if (n < file_pic_count)
            {
                file_load_pic_name(file_pic_idx[n], name);
                OLED_SetCursor(0U, (uint8_t)(r * 8U));
                if (r == sel) OLED_PrintString(">>");
                OLED_PrintString(name);
            }
        }
    }

    OLED_SetCursor(0, 48);
    OLED_PrintString("#:VIEW 0:REN");
    OLED_SetCursor(0, 56);
    OLED_PrintString("< ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);
    OLED_PrintString(" >");
    OLED_Display();
}

static void file_music_render(uint8_t page, uint8_t sel)
{
    uint8_t pages = (file_mus_count + FILE_PAGE_ROWS - 1U) / FILE_PAGE_ROWS;
    uint8_t used_on_page;
    char name[16];

    if (pages == 0U) pages = 1U;

    used_on_page = ((uint16_t)page * FILE_PAGE_ROWS < file_mus_count)
                 ? (uint8_t)(file_mus_count - (uint16_t)page * FILE_PAGE_ROWS)
                 : 0U;
    if (used_on_page > FILE_PAGE_ROWS) used_on_page = FILE_PAGE_ROWS;
    if (sel > used_on_page) sel = used_on_page;

    OLED_Clear();

    if (file_mus_count == 0U)
    {
        OLED_SetCursor(0, 8);
        OLED_PrintString("No Music");
    }
    else
    {
        for (uint8_t r = 0; r < FILE_PAGE_ROWS; r++)
        {
            uint16_t n = (uint16_t)page * FILE_PAGE_ROWS + r;
            if (n < file_mus_count)
            {
                file_load_music_name(file_mus_idx[n], name);
                OLED_SetCursor(0U, (uint8_t)(r * 8U));
                if (r == sel) OLED_PrintString(">>");
                OLED_PrintString(name);
            }
        }
    }

    OLED_SetCursor(0, 48);
    OLED_PrintString("#:PLAY *:BACK");
    OLED_SetCursor(0, 56);
    OLED_PrintString("< ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);
    OLED_PrintString(" >");
    OLED_Display();
}

static void file_view_picture(void)
{
    char key;

    OLED_Blit(file_draw.data);
    OLED_Display();

    for (;;)
    {
        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) == osOK)
        {
            if (key == '*') return;
        }
    }
}

static void file_rename_picture(uint8_t idx)
{
    if (SFlash_LoadSector(DRAW_SECTOR_BASE + idx, (uint8_t *)&file_draw, sizeof(DrawFile_t)) != SFLASH_OK)
    {
        return;
    }

    if (NameEdit_Run(file_draw.name, (uint8_t)sizeof(file_draw.name), 1U))
    {
        if (SFlash_SaveSector(DRAW_SECTOR_BASE + idx, (const uint8_t *)&file_draw, sizeof(DrawFile_t)) == SFLASH_OK)
        {
            OLED_Clear();
            OLED_SetCursor(16, 28);
            OLED_PrintString("RENAMED");
            OLED_Display();
            osDelay(300U);
        }
    }
}

static void file_picture_menu(void)
{
    uint8_t page = 0U;
    uint8_t sel = 0U;
    char key;

    file_scan_pictures();

    for (;;)
    {
        uint8_t pages = (file_pic_count + FILE_PAGE_ROWS - 1U) / FILE_PAGE_ROWS;
        uint8_t used_on_page;
        uint16_t n;

        if (pages == 0U) pages = 1U;
        if (page >= pages) page = (uint8_t)(pages - 1U);

        n = (uint16_t)page * FILE_PAGE_ROWS;
        used_on_page = (n < file_pic_count) ? (uint8_t)(file_pic_count - n) : 0U;
        if (used_on_page > FILE_PAGE_ROWS) used_on_page = FILE_PAGE_ROWS;
        if (sel > used_on_page) sel = used_on_page;

        file_pic_render(page, sel);

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        switch (key)
        {
            case '2':
                sel = (sel > 0U) ? (uint8_t)(sel - 1U)
                     : ((used_on_page > 0U) ? (uint8_t)(used_on_page - 1U) : 0U);
                break;
            case '8':
                sel = (used_on_page > 0U && sel + 1U < used_on_page)
                    ? (uint8_t)(sel + 1U) : 0U;
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
                    uint8_t idx = file_pic_idx[n + sel];
                    if (SFlash_LoadSector(DRAW_SECTOR_BASE + idx, (uint8_t *)&file_draw, sizeof(DrawFile_t)) == SFLASH_OK)
                    {
                        file_view_picture();
                    }
                }
                break;
            case '0':
                if (sel < used_on_page)
                {
                    file_rename_picture(file_pic_idx[n + sel]);
                }
                break;
            case 'D':
                if (sel < used_on_page)
                {
                    uint8_t idx = file_pic_idx[n + sel];
                    char name[16];

                    file_load_pic_name(idx, name);
                    if (Confirm_Delete(name))
                    {
                        SFlash_EraseSector(DRAW_SECTOR_BASE + idx);
                        file_scan_pictures();
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

static void file_music_menu(void)
{
    uint8_t page = 0U;
    uint8_t sel = 0U;
    char key;

    file_scan_music();

    for (;;)
    {
        uint8_t pages = (file_mus_count + FILE_PAGE_ROWS - 1U) / FILE_PAGE_ROWS;
        uint8_t used_on_page;
        uint16_t n;

        if (pages == 0U) pages = 1U;
        if (page >= pages) page = (uint8_t)(pages - 1U);

        n = (uint16_t)page * FILE_PAGE_ROWS;
        used_on_page = (n < file_mus_count) ? (uint8_t)(file_mus_count - n) : 0U;
        if (used_on_page > FILE_PAGE_ROWS) used_on_page = FILE_PAGE_ROWS;
        if (sel > used_on_page) sel = used_on_page;

        file_music_render(page, sel);

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        switch (key)
        {
            case '2':
                sel = (sel > 0U) ? (uint8_t)(sel - 1U)
                     : ((used_on_page > 0U) ? (uint8_t)(used_on_page - 1U) : 0U);
                break;
            case '8':
                sel = (used_on_page > 0U && sel + 1U < used_on_page)
                    ? (uint8_t)(sel + 1U) : 0U;
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
                    (void)file_music;   /* player not implemented yet */
                    OLED_Clear();
                    OLED_SetCursor(20, 28);
                    OLED_PrintString("No Player");
                    OLED_Display();
                    osDelay(500U);
                }
                break;
            case '*':
                return;
            default:
                break;
        }
    }
}

static void file_main_menu_render(uint8_t sel)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("FILE");
    OLED_SetCursor(0, 16);
    OLED_PrintString(sel == 0U ? "> MUSIC" : "  MUSIC");
    OLED_SetCursor(0, 24);
    OLED_PrintString(sel == 1U ? "> PICTURE" : "  PICTURE");
    OLED_SetCursor(0, 56);
    OLED_PrintString("#:OK  *:BACK");
    OLED_Display();
}

static void file_main_menu(void)
{
    uint8_t sel = 0U;
    char key;

    for (;;)
    {
        file_main_menu_render(sel);

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        switch (key)
        {
            case '2':
            case '8':
                sel ^= 1U;
                break;
            case '#':
                if (sel == 0U) file_music_menu();
                else file_picture_menu();
                break;
            case '*':
                return;
            default:
                break;
        }
    }
}

void App_File_Task_Sys(void)
{
    osThreadSuspend(osThreadGetId());

    for (;;)
    {
        file_main_menu();

        osThreadResume(oledHandle);
        osThreadSuspend(osThreadGetId());
    }
}
