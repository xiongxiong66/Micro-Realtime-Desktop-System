/**
  ******************************************************************************
  * @file           : Set_Sys.c
  * @brief          : Settings app with W25Q64 persistence
  ******************************************************************************
  */

#include "Set_Sys.h"
#include "Oled_Sys.h"
#include "Music_Sys.h"
#include "Monitor_Sys.h"
#include "Log_Sys.h"
#include "buzzer.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include <string.h>

#define SETTINGS_RECORD_SIZE  22U

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
    if (g_set_config.volume >= 10U) g_set_config.volume = 9U;
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
    g_set_config.volume = 9U;
    g_set_config.screen_timeout = 2U;
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
    rec.reserved = 0U;
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
    Buzzer_SetVolumePermille((uint16_t)(g_set_config.volume * 5U));
}

uint8_t Set_Sys_GetCursorSize(void)
{
    return g_set_config.cursor_size;
}

uint8_t Set_Sys_GetCursorPixels(void)
{
    static const uint8_t sizes[] = {1U, 2U, 5U};

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

static void Set_Render(uint8_t sel)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("SET");

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
    OLED_PrintString("/9");

    OLED_SetCursor(0, 40);
    OLED_PrintString(sel == 4U ? ">" : " ");
    OLED_PrintString("SCREEN ");
    OLED_PrintNum((uint32_t)(10U + (uint32_t)g_set_config.screen_timeout * 5U), 10);
    OLED_PrintString("s");

    OLED_SetCursor(0, 48);
    OLED_PrintString(sel == 5U ? ">" : " ");
    OLED_PrintString("PASS");

    OLED_SetCursor(0, 56);
    OLED_PrintString("4/6:CHG *:BACK");
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

    max_level = (sel == 3U) ? 9U : (sel == 4U) ? 5U : SETTINGS_LEVELS;
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

static uint8_t Set_EnterPin(char *buf, uint8_t max_len, const char *title)
{
    uint8_t len = 0U;
    char key;

    buf[0] = '\0';
    while (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK) { }

    for (;;)
    {
        OLED_Clear();
        OLED_SetCursor(0, 8);
        OLED_PrintString(title);
        OLED_SetCursor(0, 16);
        OLED_PrintString(buf);
        OLED_PrintChar('_');
        OLED_SetCursor(0, 56);
        OLED_PrintString("#:OK *:DEL");
        OLED_Display();

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
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
                return 0U;
            }
        }
        else if (key == '#')
        {
            if (len > 0U)
            {
                buf[len] = '\0';
                return 1U;
            }
        }
    }
}

void Set_Sys_Run(void)
{
    uint8_t sel = 0U;
    char key;

    Set_Sys_ApplyBrightness();

    for (;;)
    {
        Set_Render(sel);

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        switch (key)
        {
            case '2':
                sel = (sel > 0U) ? (uint8_t)(sel - 1U) : 5U;
                break;
            case '8':
                sel = (sel + 1U < 6U) ? (uint8_t)(sel + 1U) : 0U;
                break;
            case '4':
                if (sel < 5U) Set_Change(sel, -1);
                break;
            case '6':
                if (sel < 5U) Set_Change(sel, 1);
                break;
            case '#':
                if (sel == 5U)
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
