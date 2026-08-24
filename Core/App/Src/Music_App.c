/**
  ******************************************************************************
  * @file           : Music_App.c
  * @brief          : Music app: file list and playback placeholder
  ******************************************************************************
  */

#include "Music_App.h"
#include "Oled_App.h"
#include "MusicFile.h"
#include "NameEdit_App.h"
#include "Confirm_App.h"
#include "buzzer.h"
#include "Log_App.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include <string.h>

#define MUSIC_PAGE_ROWS     6U
#define MUSIC_NOTE_GAP_MS   5U
#define MUSIC_TEMPO_NUM     5U
#define MUSIC_TEMPO_DEN     4U
#define MUSIC_SCRATCH_SECTOR 1024U
#define MUSIC_COPY_CHUNK     64U
#define MUSIC_BG_FLAG_UPDATE 0x01U

static uint8_t music_used[MUSIC_SECTOR_COUNT];
static uint16_t music_used_count;
static uint8_t music_copy_buf[MUSIC_COPY_CHUNK];
static uint8_t music_flash_ok;
static volatile uint8_t music_bg_active;
static volatile uint8_t music_bg_paused;
static volatile uint8_t music_bg_restart;
static volatile uint8_t music_bg_idx;
static uint16_t music_bg_count;
static uint16_t music_bg_cur;
static uint32_t music_bg_total_ms;
static uint32_t music_bg_elapsed_ms;

static void music_scan(void)
{
    uint8_t hdr[MUSIC_HEADER_BYTES];

    music_used_count = 0U;
    for (uint16_t i = 0U; i < MUSIC_SECTOR_COUNT; i++)
    {
        if (SFlash_Read((uint32_t)(MUSIC_SECTOR_BASE + i) * SFLASH_SECTOR_SIZE,
                        hdr, MUSIC_HEADER_BYTES) == SFLASH_OK
         && hdr[0] == MUSIC_MAGIC0 && hdr[1] == MUSIC_MAGIC1
         && hdr[2] == MUSIC_MAGIC2 && hdr[3] == MUSIC_MAGIC3)
        {
            music_used[music_used_count++] = (uint8_t)i;
        }
    }
}

static void music_load_name(uint8_t idx, char *name)
{
    uint8_t hdr[MUSIC_HEADER_BYTES];
    uint8_t i;

    if (SFlash_Read((uint32_t)(MUSIC_SECTOR_BASE + idx) * SFLASH_SECTOR_SIZE,
                    hdr, MUSIC_HEADER_BYTES) != SFLASH_OK
     || hdr[0] != MUSIC_MAGIC0 || hdr[1] != MUSIC_MAGIC1
     || hdr[2] != MUSIC_MAGIC2 || hdr[3] != MUSIC_MAGIC3)
    {
        name[0] = '\0';
        return;
    }

    for (i = 0U; i < MUSIC_NAME_LEN - 1U; i++)
    {
        name[i] = (char)hdr[4U + i];
        if (name[i] == '\0') break;
    }
    name[i] = '\0';
}

static uint16_t music_load_count(uint8_t idx)
{
    uint8_t hdr[MUSIC_HEADER_BYTES];

    if (SFlash_Read((uint32_t)(MUSIC_SECTOR_BASE + idx) * SFLASH_SECTOR_SIZE,
                    hdr, MUSIC_HEADER_BYTES) != SFLASH_OK
     || hdr[0] != MUSIC_MAGIC0 || hdr[1] != MUSIC_MAGIC1
     || hdr[2] != MUSIC_MAGIC2 || hdr[3] != MUSIC_MAGIC3)
    {
        return 0U;
    }

    return (uint16_t)((uint16_t)hdr[16U] | ((uint16_t)hdr[17U] << 8U));
}

static uint8_t music_load_event(uint8_t idx, uint16_t event_idx, MusicEvent_t *ev)
{
    uint8_t raw[4];
    uint32_t addr = (uint32_t)(MUSIC_SECTOR_BASE + idx) * SFLASH_SECTOR_SIZE
                  + MUSIC_HEADER_BYTES
                  + (uint32_t)event_idx * (uint32_t)sizeof(MusicEvent_t);

    if (ev == NULL || SFlash_Read(addr, raw, sizeof(raw)) != SFLASH_OK) return 0U;

    ev->frequency_hz = (uint16_t)((uint16_t)raw[0U] | ((uint16_t)raw[1U] << 8U));
    ev->duration_ms = (uint16_t)((uint16_t)raw[2U] | ((uint16_t)raw[3U] << 8U));
    return 1U;
}

static void music_render(uint8_t page, uint8_t sel)
{
    uint8_t pages = (music_used_count + MUSIC_PAGE_ROWS - 1U) / MUSIC_PAGE_ROWS;
    uint8_t used_on_page;
    char name[16];

    if (pages == 0U) pages = 1U;

    used_on_page = ((uint16_t)page * MUSIC_PAGE_ROWS < music_used_count)
                 ? (uint8_t)(music_used_count - (uint16_t)page * MUSIC_PAGE_ROWS)
                 : 0U;
    if (used_on_page > MUSIC_PAGE_ROWS) used_on_page = MUSIC_PAGE_ROWS;
    if (sel > used_on_page) sel = used_on_page;

    OLED_Clear();

    if (music_used_count == 0U)
    {
        OLED_SetCursor(0, 8);
        OLED_PrintString(music_flash_ok ? "No Music" : "No Flash");
    }
    else
    {
        for (uint8_t r = 0U; r < MUSIC_PAGE_ROWS; r++)
        {
            uint16_t n = (uint16_t)page * MUSIC_PAGE_ROWS + r;
            if (n < music_used_count)
            {
                music_load_name(music_used[n], name);
                OLED_SetCursor(0U, (uint8_t)(r * 8U));
                if (r == sel) OLED_PrintString(">>");
                OLED_PrintString(name);
            }
        }
    }

    if (music_used_count > 0U)
    {
        OLED_SetCursor(0, 48);
        OLED_PrintString("0:REN D:DEL");
    }

    OLED_SetCursor(0, 56);
    OLED_PrintString("< ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);
    OLED_PrintString(" >");
    OLED_Display();
}

void Music_Bg_Start(uint8_t idx)
{
    music_bg_idx = idx;
    music_bg_active = 1U;
    music_bg_paused = 0U;
    music_bg_restart = 1U;
    Log_Write(LOG_TYPE_MUSIC, "PLAY");

    if (MusicPlayHandle != NULL)
    {
        osThreadFlagsSet(MusicPlayHandle, MUSIC_BG_FLAG_UPDATE);
    }
}

void Music_Bg_Prepare(uint8_t idx)
{
    music_bg_idx = idx;
    music_bg_active = 1U;
    music_bg_paused = 1U;
    music_bg_restart = 1U;

    if (MusicPlayHandle != NULL)
    {
        osThreadFlagsSet(MusicPlayHandle, MUSIC_BG_FLAG_UPDATE);
    }
}

void Music_Bg_Toggle(void)
{
    if (music_bg_active == 0U) return;

    music_bg_paused = (uint8_t)(music_bg_paused ^ 1U);
    Log_Write(LOG_TYPE_MUSIC, music_bg_paused ? "PAUSE" : "RESUME");

    if (MusicPlayHandle != NULL)
    {
        osThreadFlagsSet(MusicPlayHandle, MUSIC_BG_FLAG_UPDATE);
    }
}

uint8_t Music_Bg_IsPlaying(void)
{
    return (music_bg_active != 0U && music_bg_paused == 0U) ? 1U : 0U;
}

uint32_t Music_Bg_GetElapsedMs(void)
{
    return music_bg_elapsed_ms;
}

uint32_t Music_Bg_GetTotalMs(void)
{
    return music_bg_total_ms;
}

static void Music_Play_Render(const char *name)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString(name);
    OLED_SetCursor(0, 16);
    OLED_PrintString(Music_Bg_IsPlaying() ? "|| Playing" : "|> Paused");
    OLED_SetCursor(0, 32);
    OLED_PrintNum(Music_Bg_GetElapsedMs() / 1000U, 10);
    OLED_PrintString("s / ");
    OLED_PrintNum(Music_Bg_GetTotalMs() / 1000U, 10);
    OLED_PrintString("s");
    OLED_SetCursor(0, 56);
    OLED_PrintString(Music_Bg_IsPlaying() ? "1:PAUSE *:BACK" : "1:PLAY *:BACK");
    OLED_Display();
}

void Music_Play(uint8_t idx)
{
    char name[16];
    char key;
    uint32_t last_render = 0U;

    Music_Bg_Prepare(idx);
    music_load_name(idx, name);

    for (;;)
    {
        uint32_t now = HAL_GetTick();

        if ((now - last_render) >= 500U)
        {
            last_render = now;
            Music_Play_Render(name);
        }

        if (osMessageQueueGet(KeyHandle, &key, NULL, 100U) == osOK)
        {
            if (key == '1')
            {
                Music_Bg_Toggle();
                Music_Play_Render(name);
            }
            else if (key == '*')
            {
                return;
            }
        }
    }
}

void Music_Play_Task_Sys(void)
{
    Buzzer_Init();

    for (;;)
    {
        if (music_bg_active != 0U && music_bg_paused == 0U)
        {
            MusicEvent_t ev;
            uint32_t flags;
            uint16_t dur = 10U;

            if (music_bg_restart)
            {
                music_bg_restart = 0U;
                music_bg_cur = 0U;
                music_bg_elapsed_ms = 0U;
                music_bg_total_ms = 0U;
                music_bg_count = music_load_count(music_bg_idx);

                for (uint16_t i = 0U; i < music_bg_count; i++)
                {
                    MusicEvent_t tev;
                    if (music_load_event(music_bg_idx, i, &tev))
                    {
                        uint16_t tdur = tev.duration_ms;
                        if (tdur == 0U) tdur = 10U;
                        tdur = (uint16_t)(((uint32_t)tdur * MUSIC_TEMPO_NUM) / MUSIC_TEMPO_DEN);
                        music_bg_total_ms += tdur;
                        if (i + 1U < music_bg_count) music_bg_total_ms += MUSIC_NOTE_GAP_MS;
                    }
                }
            }

            if (music_bg_count == 0U)
            {
                music_bg_active = 0U;
                Buzzer_Stop();
                osThreadFlagsWait(MUSIC_BG_FLAG_UPDATE, osFlagsWaitAny, osWaitForever);
                continue;
            }

            if (music_bg_cur >= music_bg_count) music_bg_cur = 0U;

            if (music_load_event(music_bg_idx, music_bg_cur, &ev))
            {
                if (ev.frequency_hz > 0U) Buzzer_SetFrequency(ev.frequency_hz);
                else Buzzer_Stop();

                dur = ev.duration_ms;
                if (dur == 0U) dur = 10U;
                dur = (uint16_t)(((uint32_t)dur * MUSIC_TEMPO_NUM) / MUSIC_TEMPO_DEN);
                if (dur == 0U) dur = 10U;
                music_bg_elapsed_ms += dur;
            }
            else
            {
                Buzzer_Stop();
            }

            flags = osThreadFlagsWait(MUSIC_BG_FLAG_UPDATE, osFlagsWaitAny, (uint32_t)dur);

            if ((flags & MUSIC_BG_FLAG_UPDATE) != 0U)
            {
                Buzzer_Stop();
                continue;
            }

            if (music_bg_active != 0U && music_bg_paused == 0U)
            {
                Buzzer_Stop();
                music_bg_cur++;
                if (music_bg_cur >= music_bg_count)
                {
                    music_bg_cur = 0U;
                    music_bg_elapsed_ms = 0U;
                    music_bg_paused = 1U;
                }
                else
                {
                    osDelay(MUSIC_NOTE_GAP_MS);
                    music_bg_elapsed_ms += MUSIC_NOTE_GAP_MS;
                }
            }
        }
        else
        {
            Buzzer_Stop();
            osThreadFlagsWait(MUSIC_BG_FLAG_UPDATE, osFlagsWaitAny, osWaitForever);
        }
    }
}

static SFlash_Status_t music_copy_range(uint32_t src_addr, uint32_t dst_addr, uint32_t len)
{
    while (len > 0U)
    {
        uint32_t chunk = (len > MUSIC_COPY_CHUNK) ? MUSIC_COPY_CHUNK : len;
        SFlash_Status_t st;

        st = SFlash_Read(src_addr, music_copy_buf, chunk);
        if (st != SFLASH_OK) return st;
        st = SFlash_Write(dst_addr, music_copy_buf, chunk);
        if (st != SFLASH_OK) return st;

        src_addr += chunk;
        dst_addr += chunk;
        len -= chunk;
    }

    return SFLASH_OK;
}

void Music_Rename(uint8_t idx)
{
    char name[MUSIC_NAME_LEN];
    uint8_t hdr[MUSIC_HEADER_BYTES];
    uint8_t name_len;
    uint32_t src = (uint32_t)(MUSIC_SECTOR_BASE + idx) * SFLASH_SECTOR_SIZE;
    uint32_t scratch = (uint32_t)MUSIC_SCRATCH_SECTOR * SFLASH_SECTOR_SIZE;

    music_load_name(idx, name);
    if (NameEdit_Run(name, MUSIC_NAME_LEN, 1U, Music_NameUsed) != 1U) return;

    if (SFlash_Read(src, hdr, MUSIC_HEADER_BYTES) != SFLASH_OK) return;

    memset(&hdr[4U], 0, MUSIC_NAME_LEN);
    name_len = (uint8_t)(strlen(name) + 1U);
    if (name_len > MUSIC_NAME_LEN) name_len = MUSIC_NAME_LEN;
    memcpy(&hdr[4U], name, name_len);

    if (SFlash_EraseSector(MUSIC_SCRATCH_SECTOR) != SFLASH_OK) return;
    if (SFlash_Write(scratch, hdr, MUSIC_HEADER_BYTES) != SFLASH_OK) return;
    if (music_copy_range(src + MUSIC_HEADER_BYTES, scratch + MUSIC_HEADER_BYTES,
                         SFLASH_SECTOR_SIZE - MUSIC_HEADER_BYTES) != SFLASH_OK)
    {
        return;
    }

    if (SFlash_EraseSector(MUSIC_SECTOR_BASE + idx) != SFLASH_OK) return;
    if (music_copy_range(scratch, src, SFLASH_SECTOR_SIZE) != SFLASH_OK) return;

    OLED_Clear();
    OLED_SetCursor(16, 28);
    OLED_PrintString("RENAMED");
    OLED_Display();
    osDelay(300U);
    Log_Write(LOG_TYPE_MUSIC, "RENAMED");
}

uint8_t Music_NameUsed(const char *new_name, const char *old_name)
{
    char other[MUSIC_NAME_LEN];
    uint16_t i;

    if (new_name == NULL || new_name[0] == '\0') return 0U;

    for (i = 0U; i < MUSIC_SECTOR_COUNT; i++)
    {
        music_load_name((uint8_t)i, other);
        if (other[0] != '\0'
         && strcmp(other, new_name) == 0
         && (old_name == NULL || strcmp(other, old_name) != 0))
        {
            return 1U;
        }
    }
    return 0U;
}

void Music_Delete(uint8_t idx)
{
    char name[16];

    music_load_name(idx, name);
    if (Confirm_Delete(name))
    {
        if (SFlash_EraseSector(MUSIC_SECTOR_BASE + idx) == SFLASH_OK)
        {
            Log_Write(LOG_TYPE_MUSIC, "DELETED");
        }
    }
}

static void music_selection(void)
{
    uint8_t page = 0U;
    uint8_t sel = 0U;
    char key;

    music_flash_ok = (SFlash_Init() == SFLASH_OK);

    for (;;)
    {
        uint8_t pages;
        uint8_t used_on_page;
        uint16_t n;

        music_scan();

        pages = (music_used_count + MUSIC_PAGE_ROWS - 1U) / MUSIC_PAGE_ROWS;
        if (pages == 0U) pages = 1U;
        if (page >= pages) page = (uint8_t)(pages - 1U);

        n = (uint16_t)page * MUSIC_PAGE_ROWS;
        used_on_page = (n < music_used_count)
                     ? (uint8_t)(music_used_count - n)
                     : 0U;
        if (used_on_page > MUSIC_PAGE_ROWS) used_on_page = MUSIC_PAGE_ROWS;
        if (sel > used_on_page) sel = used_on_page;

        music_render(page, sel);

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
                    Music_Play(music_used[n + sel]);
                }
                break;
            case '0':
                if (sel < used_on_page)
                {
                    Music_Rename(music_used[n + sel]);
                }
                break;
            case 'D':
                if (sel < used_on_page)
                {
                    Music_Delete(music_used[n + sel]);
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

void Music_App_Run(void)
{
    music_selection();
}
