/**
  ******************************************************************************
  * @file           : Set_App.c
  * @brief          : Settings app with W25Q64 persistence
  ******************************************************************************
  */

#include "Set_App.h"
#include "TaskWatch.h"
#include "Oled_App.h"
#include "Music_App.h"
#include "Monitor_App.h"
#include "Log_App.h"
#include "UiInput.h"
#include "buzzer.h"
#include "DS3231.h"
#include "Screen_App.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include <string.h>

#define SETTINGS_RECORD_SIZE  22U
#define SETTINGS_VERSION      1U

#define SETTINGS_MAGIC0 'S'
#define SETTINGS_MAGIC1 'E'
#define SETTINGS_MAGIC2 'T'
#define SETTINGS_MAGIC3 '1'

typedef struct {
    uint8_t magic[4];
    uint8_t cursor_size;
    uint8_t sensitivity;
    uint8_t brightness;
    uint8_t volume;
    uint8_t screen_timeout;
    char password[SETTINGS_PIN_MAX_LEN + 1U];
    uint8_t reserved;
    uint8_t checksum;
} SetRecord_t;

static const uint8_t set_lock_seconds_tab[SETTINGS_LOCK_LEVELS] = {
    45U, 60U, 120U
};

static SetConfig_t g_set_config;
static uint8_t set_load_valid;

static uint8_t Set_Checksum(const SetRecord_t *rec)
{
    uint8_t sum = 0U;
    const uint8_t *p = (const uint8_t *)rec;

    for (uint8_t i = 0U; i < SETTINGS_RECORD_SIZE - 1U; i++)
    {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t Set_RecordValid(const SetRecord_t *rec)
{
    return (rec->magic[0] == SETTINGS_MAGIC0
         && rec->magic[1] == SETTINGS_MAGIC1
         && rec->magic[2] == SETTINGS_MAGIC2
         && rec->magic[3] == SETTINGS_MAGIC3
         && rec->checksum == Set_Checksum(rec)) ? 1U : 0U;
}

static void Set_RecordToConfig(const SetRecord_t *rec)
{
    uint8_t packed = rec->reserved;

    g_set_config.cursor_size = rec->cursor_size;
    g_set_config.sensitivity = rec->sensitivity;
    g_set_config.brightness = rec->brightness;
    g_set_config.volume = rec->volume;
    g_set_config.screen_timeout = rec->screen_timeout;
    memcpy(g_set_config.password, rec->password, SETTINGS_PIN_MAX_LEN + 1U);
    g_set_config.password[SETTINGS_PIN_MAX_LEN] = '\0';

    if (g_set_config.cursor_size >= SETTINGS_LEVELS) g_set_config.cursor_size = 1U;
    if (g_set_config.sensitivity >= SETTINGS_LEVELS) g_set_config.sensitivity = 1U;
    if (g_set_config.brightness >= SETTINGS_LEVELS) g_set_config.brightness = 1U;
    if (packed >= 0x10U)
    {
        if ((packed >> 4U) != SETTINGS_VERSION)
        {
            /* 新版但版本不匹配：0 静音、1 极小、其余归并到最大档 */
            if (g_set_config.volume > 1U) g_set_config.volume = 5U;
        }
        g_set_config.lock_delay = (uint8_t)(packed & 0x0FU);
        if (g_set_config.lock_delay >= SETTINGS_LOCK_LEVELS)
        {
            g_set_config.lock_delay = 0U;
        }
    }
    else
    {
        /* 旧版记录 reserved 直接存版本号，没有锁屏字段 */
        if (packed != SETTINGS_VERSION)
        {
            if (g_set_config.volume > 1U) g_set_config.volume = 5U;
        }
        g_set_config.lock_delay = 0U;
    }
    if (g_set_config.volume >= 6U) g_set_config.volume = 5U;
    if (g_set_config.screen_timeout >= 5U) g_set_config.screen_timeout = 2U;
}

void Set_Sys_Load(void)
{
    SetRecord_t rec;
    uint32_t addr = (uint32_t)SETTINGS_SECTOR_BASE * SFLASH_SECTOR_SIZE;

    for (uint8_t attempt = 0U; attempt < 5U; attempt++)
    {
        if (SFlash_Init() != SFLASH_OK)
        {
            HAL_Delay(50U);
            continue;
        }
        if (SFlash_Read(addr, (uint8_t *)&rec, SETTINGS_RECORD_SIZE) != SFLASH_OK)
        {
            HAL_Delay(50U);
            continue;
        }
        if (Set_RecordValid(&rec))
        {
            set_load_valid = 1U;
            Set_RecordToConfig(&rec);
            return;
        }
        break;
    }

    set_load_valid = 0U;
    Monitor_Sys_ReportError();

    g_set_config.cursor_size = 1U;
    g_set_config.sensitivity = 1U;
    g_set_config.brightness = 1U;
    g_set_config.volume = 5U;
    g_set_config.screen_timeout = 2U;
    g_set_config.lock_delay = 0U;
    memset(g_set_config.password, 0, sizeof(g_set_config.password));
    strcpy(g_set_config.password, "12345");
    Set_Sys_Save();
}

void Set_Sys_Save(void)
{
    SetRecord_t rec;
    uint8_t saved = 0U;
    uint32_t addr = (uint32_t)SETTINGS_SECTOR_BASE * SFLASH_SECTOR_SIZE;

    rec.magic[0] = SETTINGS_MAGIC0;
    rec.magic[1] = SETTINGS_MAGIC1;
    rec.magic[2] = SETTINGS_MAGIC2;
    rec.magic[3] = SETTINGS_MAGIC3;
    rec.cursor_size = g_set_config.cursor_size;
    rec.sensitivity = g_set_config.sensitivity;
    rec.brightness = g_set_config.brightness;
    rec.volume = g_set_config.volume;
    rec.screen_timeout = g_set_config.screen_timeout;
    memset(rec.password, 0, sizeof(rec.password));
    memcpy(rec.password, g_set_config.password, SETTINGS_PIN_MAX_LEN + 1U);
    rec.reserved = (uint8_t)((SETTINGS_VERSION << 4U)
                           | (g_set_config.lock_delay & 0x0FU));
    rec.checksum = Set_Checksum(&rec);

    if (SFlash_Init() != SFLASH_OK)
    {
        Monitor_Sys_ReportError();
        Log_Write(LOG_TYPE_ERROR, "SET SAVE ERR");
        return;
    }

    for (uint8_t attempt = 0U; attempt < 2U && saved == 0U; attempt++)
    {
        SetRecord_t readback;

        if (SFlash_EraseSector(SETTINGS_SECTOR_BASE) != SFLASH_OK) continue;
        if (SFlash_Write(addr, (const uint8_t *)&rec, SETTINGS_RECORD_SIZE) != SFLASH_OK) continue;

        if (SFlash_Read(addr, (uint8_t *)&readback, SETTINGS_RECORD_SIZE) == SFLASH_OK
         && Set_RecordValid(&readback))
        {
            saved = 1U;
        }
    }

    if (saved != 0U)
    {
        Log_Write(LOG_TYPE_SETTING, "SET SAVED");
    }
    else
    {
        Monitor_Sys_ReportError();
        Log_Write(LOG_TYPE_ERROR, "SET SAVE ERR");
    }
}

void Set_Sys_ApplyBrightness(void)
{
    static const uint8_t contrast[] = {0x30U, 0x7FU, 0xFFU};

    OLED_SetContrast(contrast[g_set_config.brightness]);
}

void Set_Sys_ApplyVolume(void)
{
    static const uint16_t volume_tab[6] = {0U, 10U, 30U, 80U, 200U, 500U};
    uint8_t v = (g_set_config.volume < 6U) ? g_set_config.volume : 5U;

    Buzzer_SetVolumePermille(volume_tab[v]);
}

uint8_t Set_Sys_GetCursorSize(void)
{
    return g_set_config.cursor_size;
}

uint8_t Set_Sys_GetCursorPixels(void)
{
    static const uint8_t sizes[] = {1U, 4U, 5U};

    if (g_set_config.cursor_size >= 3U) return 2U;
    return sizes[g_set_config.cursor_size];
}

uint8_t Set_Sys_GetSensitivity(void)
{
    return g_set_config.sensitivity;
}

uint8_t Set_Sys_GetBrightness(void)
{
    return g_set_config.brightness;
}

uint8_t Set_Sys_GetVolume(void)
{
    return g_set_config.volume;
}

uint32_t Set_Sys_GetScreenTimeoutMs(void)
{
    return (uint32_t)(10U + (uint32_t)g_set_config.screen_timeout * 5U) * 1000U;
}

uint32_t Set_Sys_GetLockDelayMs(void)
{
    uint8_t level = (g_set_config.lock_delay < SETTINGS_LOCK_LEVELS)
                  ? g_set_config.lock_delay : 0U;
    return (uint32_t)set_lock_seconds_tab[level] * 1000U;
}

//pin正确返回1U，pin错误或为空返回0U
uint8_t Set_Sys_CheckPin(const char *pin)
{
    return (pin != NULL && strcmp(pin, g_set_config.password) == 0) ? 1U : 0U;
}

uint8_t Set_Sys_LoadValid(void)
{
    return set_load_valid;
}

void Set_Sys_ChangePin(const char *pin)
{
    if (pin == NULL) return;

    memset(g_set_config.password, 0, sizeof(g_set_config.password));
    strncpy(g_set_config.password, pin, SETTINGS_PIN_MAX_LEN);
    g_set_config.password[SETTINGS_PIN_MAX_LEN] = '\0';
    Set_Sys_Save();
}

static void Set_RtcRender(uint8_t sel, const DS3231_Time_t *t)
{
    static const char * const names[] = {"YEAR", "MON", "DAY", "HOUR", "MIN"};
    uint8_t vals[] = {t->year, t->month, t->day, t->hour, t->minute};

    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("TIME SET");

    for (uint8_t i = 0U; i < 5U; i++)
    {
        OLED_SetCursor(0, (uint8_t)(8U + i * 8U));
        OLED_PrintString(sel == i ? ">" : " ");
        OLED_PrintString(names[i]);
        OLED_PrintChar(' ');
        OLED_PrintNum((uint32_t)vals[i], 10);
    }

    OLED_Display();
}

static void Set_RtcAdjust(DS3231_Time_t *t, uint8_t sel, int8_t dir)
{
    uint8_t *value = NULL;
    uint8_t min = 0U;
    uint8_t max = 0U;

    switch (sel)
    {
        case 0U: value = &t->year;   max = 99U; break;
        case 1U: value = &t->month;  min = 1U;  max = 12U; break;
        case 2U: value = &t->day;    min = 1U;  max = 31U; break;
        case 3U: value = &t->hour;   max = 23U; break;
        case 4U: value = &t->minute; max = 59U; break;
        default: return;
    }

    if (dir > 0)
    {
        *value = (*value < max) ? (uint8_t)(*value + 1U) : min;
    }
    else
    {
        *value = (*value > min) ? (uint8_t)(*value - 1U) : max;
    }
}

static void Set_RtcSet(void)
{
    DS3231_Time_t t;
    uint8_t sel = 0U;
    char key;

    if (!DS3231_ReadTime(&t)) return;

    for (;;)
    {
        Set_RtcRender(sel, &t);

        if (Ui_KeyGet(&key, osWaitForever) != osOK)
        {
            continue;
        }

        TaskWatch_Beat(TASKWATCH_OLED);

        if (key == '2') sel = (sel > 0U) ? (uint8_t)(sel - 1U) : 4U;
        else if (key == '8') sel = (sel < 4U) ? (uint8_t)(sel + 1U) : 0U;
        else if (key == '4') Set_RtcAdjust(&t, sel, -1);
        else if (key == '6') Set_RtcAdjust(&t, sel, 1);
        else if (key == '#')
        {
            t.second = 0U;
            if (DS3231_WriteTime(&t))
            {
                OLED_Clear();
                OLED_SetCursor(16, 28);
                OLED_PrintString("SAVED");
                OLED_Display();
                osDelay(300U);
            }
            return;
        }
        else if (key == '*')
        {
            return;
        }
    }
}

static void Set_Render(uint8_t page, uint8_t sel)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString(page == 0U ? "SET 1/2" : "SET 2/2");

    if (page == 0U)
    {
        OLED_SetCursor(0, 8);
        OLED_PrintString(sel == 0U ? ">" : " ");
        OLED_PrintString("SIZE ");
        OLED_PrintNum((uint32_t)g_set_config.cursor_size + 1U, 10);
        OLED_PrintString("/3");

        OLED_SetCursor(0, 16);
        OLED_PrintString(sel == 1U ? ">" : " ");
        OLED_PrintString("SPEED ");
        OLED_PrintNum((uint32_t)g_set_config.sensitivity + 1U, 10);
        OLED_PrintString("/3");

        OLED_SetCursor(0, 24);
        OLED_PrintString(sel == 2U ? ">" : " ");
        OLED_PrintString("BRIGHT ");
        OLED_PrintNum((uint32_t)g_set_config.brightness + 1U, 10);
        OLED_PrintString("/3");

        OLED_SetCursor(0, 32);
        OLED_PrintString(sel == 3U ? ">" : " ");
        OLED_PrintString("VOLUME ");
        OLED_PrintNum((uint32_t)g_set_config.volume, 10);
        OLED_PrintString("/5");

        OLED_SetCursor(0, 40);
        OLED_PrintString(sel == 4U ? ">" : " ");
        OLED_PrintString("SCREEN ");
        OLED_PrintNum((uint32_t)(10U + (uint32_t)g_set_config.screen_timeout * 5U), 10);
        OLED_PrintString("s");

    }
    else
    {
        OLED_SetCursor(0, 8);
        OLED_PrintString(sel == 0U ? ">" : " ");
        OLED_PrintString("PASS");

        OLED_SetCursor(0, 16);
        OLED_PrintString(sel == 1U ? ">" : " ");
        OLED_PrintString("TIME");

        OLED_SetCursor(0, 24);
        OLED_PrintString(sel == 2U ? ">" : " ");
        OLED_PrintString("LOCK ");
        OLED_PrintNum((uint32_t)set_lock_seconds_tab[g_set_config.lock_delay], 10);
        OLED_PrintString("s");
    }
    OLED_Display();
}

static void Set_Change(uint8_t sel, int8_t dir)
{
    uint8_t max_level;
    uint8_t *value = (sel == 0U) ? &g_set_config.cursor_size
                   : (sel == 1U) ? &g_set_config.sensitivity
                   : (sel == 2U) ? &g_set_config.brightness
                   : (sel == 3U) ? &g_set_config.volume
                   : &g_set_config.screen_timeout;

    if (sel >= 5U) return;

    max_level = (sel == 3U) ? 6U : (sel == 4U) ? 5U : SETTINGS_LEVELS;
    if (dir > 0)
    {
        *value = (*value + 1U < max_level) ? (uint8_t)(*value + 1U) : 0U;
    }
    else
    {
        *value = (*value > 0U) ? (uint8_t)(*value - 1U) : (uint8_t)(max_level - 1U);
    }

    if (sel == 2U) Set_Sys_ApplyBrightness();
    if (sel == 3U) Set_Sys_ApplyVolume();
    Set_Sys_Save();
}

static void Set_ChangeLock(int8_t dir)
{
    if (dir > 0)
    {
        g_set_config.lock_delay = (g_set_config.lock_delay + 1U < SETTINGS_LOCK_LEVELS)
                                ? (uint8_t)(g_set_config.lock_delay + 1U) : 0U;
    }
    else
    {
        g_set_config.lock_delay = (g_set_config.lock_delay > 0U)
                                ? (uint8_t)(g_set_config.lock_delay - 1U)
                                : (uint8_t)(SETTINGS_LOCK_LEVELS - 1U);
    }
    Set_Sys_Save();
}

static uint8_t Set_EnterPin(char *buf, uint8_t max_len, const char *title)
{
    uint8_t len = 0U;
    char key;

    buf[0] = '\0';
    Screen_Sys_SetForceOff(0U);
    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }

    for (;;)
    {
        OLED_Clear();
        OLED_SetCursor(0, 8);
        OLED_PrintString(title);
        OLED_SetCursor(0, 16);
        OLED_PrintString(buf);
        OLED_PrintChar('_');
        OLED_Display();

        if (Ui_KeyGet(&key, osWaitForever) != osOK)
        {
            continue;
        }

        if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D'))
        {
            if (len < max_len - 1U)
            {
                buf[len++] = key;
                buf[len] = '\0';
            }
        }
        else if (key == '*')
        {
            if (len > 0U)
            {
                len--;
                buf[len] = '\0';
            }
            else
            {
                Screen_Sys_SetForceOff(1U);
                return 0U;
            }
        }
        else if (key == '#')
        {
            if (len > 0U)
            {
                buf[len] = '\0';
                Screen_Sys_SetForceOff(1U);
                return 1U;
            }
        }
    }
}

void Set_Sys_Run(void)
{
    uint8_t page = 0U;
    uint8_t sel = 0U;
    char key;

    Set_Sys_ApplyBrightness();

    for (;;)
    {
        Set_Render(page, sel);

        if (Ui_KeyGet(&key, osWaitForever) != osOK)
        {
            continue;
        }

        TaskWatch_Beat(TASKWATCH_OLED);

        switch (key)
        {
            case '2':
                if (page == 0U)
                {
                    sel = (sel > 0U) ? (uint8_t)(sel - 1U) : 0U;
                }
                else
                {
                    if (sel == 0U) { page = 0U; sel = 4U; }
                    else sel = (uint8_t)(sel - 1U);
                }
                break;
            case '8':
                if (page == 0U)
                {
                    if (sel < 4U)
                    {
                        sel++;
                    }
                    else
                    {
                        page = 1U;
                        sel = 0U;
                    }
                }
                else
                {
                    sel = (sel < 2U) ? (uint8_t)(sel + 1U) : 0U;
                }
                break;
            case '4':
                if (page == 0U && sel < 5U) Set_Change(sel, -1);
                else if (page == 1U && sel == 2U) Set_ChangeLock(-1);
                break;
            case '6':
                if (page == 0U && sel < 5U) Set_Change(sel, 1);
                else if (page == 1U && sel == 2U) Set_ChangeLock(1);
                break;
            case '#':
                if (page == 1U && sel == 0U)
                {
                    char old_pin[SETTINGS_PIN_MAX_LEN + 1U];
                    char new_pin[SETTINGS_PIN_MAX_LEN + 1U];

                    if (Set_EnterPin(old_pin, sizeof(old_pin), "Old Pin"))
                    {
                        if (Set_Sys_CheckPin(old_pin))
                        {
                            if (Set_EnterPin(new_pin, sizeof(new_pin), "New Pin"))
                            {
                                Set_Sys_ChangePin(new_pin);
                                OLED_Clear();
                                OLED_SetCursor(16, 28);
                                OLED_PrintString("CHANGED");
                                OLED_Display();
                                osDelay(300U);
                            }
                        }
                        else
                        {
                            OLED_Clear();
                            OLED_SetCursor(16, 28);
                            OLED_PrintString("Wrong!");
                            OLED_Display();
                            osDelay(300U);
                        }
                    }
                }
                else if (page == 1U && sel == 1U)
                {
                    Set_RtcSet();
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
