/**
  ******************************************************************************
  * @file           : Log_Sys.c
  * @brief          : W25Q64 circular system log and viewer
  ******************************************************************************
  */

#include "Log_Sys.h"
#include "Oled_Sys.h"
#include "Music_Sys.h"
#include "Set_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "sflash.h"
#include "main.h"
#include <string.h>

#define LOG_MAGIC              0x4C4F4731UL
#define LOG_HEADER_SECTOR      800U
#define LOG_DATA_BASE          801U
#define LOG_DATA_SECTORS       223U
#define LOG_ENTRY_SIZE         32U
#define LOG_ENTRIES_PER_SECTOR (SFLASH_SECTOR_SIZE / LOG_ENTRY_SIZE)
#define LOG_QUEUE_DEPTH        8U
#define LOG_FLUSH_ENTRIES      8U
#define LOG_VIEW_MAX           24U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t seq;
    uint32_t timestamp;
    uint8_t type;
    uint8_t len;
    uint8_t data[LOG_TEXT_LEN];
    uint8_t checksum;
} LogEntry_t;

typedef struct {
    uint32_t seq;
    uint32_t timestamp;
    uint8_t type;
    char text[LOG_TEXT_LEN];
} LogView_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t next_seq;
    uint32_t cur_sector;
    uint32_t cur_offset;
    uint32_t total;
    uint8_t checksum;
} LogHeader_t;

static uint8_t log_flush_buf[LOG_FLUSH_ENTRIES * LOG_ENTRY_SIZE];
static uint8_t log_flush_count;
static uint32_t log_next_seq = 1U;
static uint32_t log_cur_sector = LOG_DATA_BASE;
static uint32_t log_cur_offset;
static uint8_t log_ready;
static LogView_t log_view[LOG_VIEW_MAX];
static uint8_t log_view_count;

static uint8_t Log_EntryChecksum(const LogEntry_t *entry)
{
    const uint8_t *p = (const uint8_t *)entry;
    uint8_t sum = 0U;

    for (uint8_t i = 0U; i < LOG_ENTRY_SIZE - 1U; i++)
    {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t Log_EntryValid(const LogEntry_t *entry)
{
    return (entry->magic == LOG_MAGIC
         && entry->seq != 0U
         && entry->len <= LOG_TEXT_LEN
         && entry->checksum == Log_EntryChecksum(entry)) ? 1U : 0U;
}

static uint8_t Log_HeaderChecksum(const LogHeader_t *header)
{
    const uint8_t *p = (const uint8_t *)header;
    uint8_t sum = 0U;

    for (uint8_t i = 0U; i < sizeof(LogHeader_t) - 1U; i++)
    {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t Log_HeaderValid(const LogHeader_t *header)
{
    return (header->magic == LOG_MAGIC
         && header->next_seq > 0U
         && header->cur_sector >= LOG_DATA_BASE
         && header->cur_sector < LOG_DATA_BASE + LOG_DATA_SECTORS
         && header->cur_offset <= SFLASH_SECTOR_SIZE
         && header->checksum == Log_HeaderChecksum(header)) ? 1U : 0U;
}

static void Log_AddView(const LogEntry_t *entry);
static void Log_SortView(void);

static void Log_SaveHeader(void)
{
    LogHeader_t h;
    uint32_t addr = (uint32_t)LOG_HEADER_SECTOR * SFLASH_SECTOR_SIZE;

    h.magic = LOG_MAGIC;
    h.next_seq = log_next_seq;
    h.cur_sector = log_cur_sector;
    h.cur_offset = log_cur_offset;
    h.total = 0U;
    h.checksum = Log_HeaderChecksum(&h);

    if (SFlash_EraseSector(LOG_HEADER_SECTOR) == SFLASH_OK)
    {
        (void)SFlash_Write(addr, (const uint8_t *)&h, sizeof(h));
    }
}

static void Log_LoadView(void)
{
    uint32_t s = log_cur_sector;
    uint32_t limit = log_cur_offset / LOG_ENTRY_SIZE;
    uint32_t scanned = 0U;

    if (limit > LOG_ENTRIES_PER_SECTOR) limit = LOG_ENTRIES_PER_SECTOR;
    log_view_count = 0U;

    while (scanned < LOG_DATA_SECTORS && log_view_count < LOG_VIEW_MAX)
    {
        for (uint32_t i = limit; i > 0U; i--)
        {
            LogEntry_t e;
            uint32_t addr = s * SFLASH_SECTOR_SIZE + (i - 1U) * LOG_ENTRY_SIZE;

            if (SFlash_Read(addr, (uint8_t *)&e, LOG_ENTRY_SIZE) == SFLASH_OK
             && Log_EntryValid(&e))
            {
                Log_AddView(&e);
            }
            if (log_view_count >= LOG_VIEW_MAX) break;
        }

        if (log_view_count >= LOG_VIEW_MAX) break;

        s = (s <= LOG_DATA_BASE) ? LOG_DATA_BASE + LOG_DATA_SECTORS - 1U : s - 1U;
        limit = LOG_ENTRIES_PER_SECTOR;
        scanned++;
    }

    Log_SortView();
}

static void Log_AddView(const LogEntry_t *entry)
{
    LogView_t v;
    uint8_t i;

    v.seq = entry->seq;
    v.timestamp = entry->timestamp;
    v.type = entry->type;
    memset(v.text, 0, sizeof(v.text));
    memcpy(v.text, entry->data, entry->len);

    if (log_view_count < LOG_VIEW_MAX)
    {
        for (i = log_view_count; i > 0U; i--)
        {
            log_view[i] = log_view[i - 1U];
        }
        log_view[0] = v;
        log_view_count++;
    }
    else if (v.seq > log_view[LOG_VIEW_MAX - 1U].seq)
    {
        for (i = LOG_VIEW_MAX - 1U; i > 0U; i--)
        {
            log_view[i] = log_view[i - 1U];
        }
        log_view[0] = v;
    }
}

static void Log_SortView(void)
{
    for (uint8_t i = 0U; i + 1U < log_view_count; i++)
    {
        for (uint8_t j = i + 1U; j < log_view_count; j++)
        {
            if (log_view[i].seq < log_view[j].seq)
            {
                LogView_t t = log_view[i];
                log_view[i] = log_view[j];
                log_view[j] = t;
            }
        }
    }
}

static void Log_Scan(void)
{
    uint32_t max_seq = 0U;

    log_next_seq = 1U;
    log_cur_sector = LOG_DATA_BASE;
    log_cur_offset = 0U;
    log_view_count = 0U;

    for (uint32_t s = LOG_DATA_BASE; s < LOG_DATA_BASE + LOG_DATA_SECTORS; s++)
    {
        for (uint32_t i = 0U; i < LOG_ENTRIES_PER_SECTOR; i++)
        {
            LogEntry_t e;
            uint32_t addr = s * SFLASH_SECTOR_SIZE + i * LOG_ENTRY_SIZE;

            if (SFlash_Read(addr, (uint8_t *)&e, LOG_ENTRY_SIZE) != SFLASH_OK) continue;
            if (!Log_EntryValid(&e)) continue;

            Log_AddView(&e);
            if (e.seq > max_seq)
            {
                max_seq = e.seq;
                log_cur_sector = s;
                log_cur_offset = (i + 1U) * LOG_ENTRY_SIZE;
            }
        }
    }

    if (max_seq == 0U)
    {
        log_cur_sector = LOG_DATA_BASE;
        log_cur_offset = 0U;
        (void)SFlash_EraseSector(LOG_DATA_BASE);
    }
    else
    {
        log_next_seq = max_seq + 1U;
        if (log_cur_offset >= SFLASH_SECTOR_SIZE)
        {
            log_cur_sector = (log_cur_sector >= LOG_DATA_BASE + LOG_DATA_SECTORS - 1U)
                           ? LOG_DATA_BASE : log_cur_sector + 1U;
            log_cur_offset = 0U;
        }
    }

    Log_SortView();
    log_ready = 1U;
}

static void Log_Init(void)
{
    LogHeader_t h;
    uint32_t addr = (uint32_t)LOG_HEADER_SECTOR * SFLASH_SECTOR_SIZE;

    if (SFlash_Init() != SFLASH_OK)
    {
        log_ready = 0U;
        return;
    }

    if (SFlash_Read(addr, (uint8_t *)&h, sizeof(h)) == SFLASH_OK
     && Log_HeaderValid(&h))
    {
        log_next_seq = h.next_seq;
        log_cur_sector = h.cur_sector;
        log_cur_offset = h.cur_offset;
        Log_LoadView();
        log_ready = 1U;
        return;
    }

    Log_Scan();
    Log_SaveHeader();
}

static void Log_NextSector(void)
{
    uint32_t next = (log_cur_sector >= LOG_DATA_BASE + LOG_DATA_SECTORS - 1U)
                  ? LOG_DATA_BASE : log_cur_sector + 1U;

    if (SFlash_EraseSector(next) == SFLASH_OK)
    {
        log_cur_sector = next;
        log_cur_offset = 0U;
        Log_SaveHeader();
    }
}

static void Log_Flush(void)
{
    uint32_t addr;

    if (log_flush_count == 0U) return;

    addr = log_cur_sector * SFLASH_SECTOR_SIZE
         + log_cur_offset - (uint32_t)log_flush_count * LOG_ENTRY_SIZE;
    (void)SFlash_Write(addr, log_flush_buf, (uint32_t)log_flush_count * LOG_ENTRY_SIZE);
    log_flush_count = 0U;
}

static void Log_Append(const LogMsg_t *msg)
{
    LogEntry_t e;
    uint8_t len = (uint8_t)strlen(msg->text);

    if (!log_ready) return;
    if (len > LOG_TEXT_LEN) len = LOG_TEXT_LEN;

    if (log_cur_offset + LOG_ENTRY_SIZE > SFLASH_SECTOR_SIZE)
    {
        Log_Flush();
        Log_NextSector();
    }

    e.magic = LOG_MAGIC;
    e.seq = log_next_seq++;
    e.timestamp = HAL_GetTick() / 1000U;
    e.type = msg->type;
    e.len = len;
    memset(e.data, 0, sizeof(e.data));
    memcpy(e.data, msg->text, len);
    e.checksum = Log_EntryChecksum(&e);

    memcpy(&log_flush_buf[(uint32_t)log_flush_count * LOG_ENTRY_SIZE], &e, LOG_ENTRY_SIZE);
    log_flush_count++;
    log_cur_offset += LOG_ENTRY_SIZE;
    Log_AddView(&e);

    if (log_flush_count >= LOG_FLUSH_ENTRIES)
    {
        Log_Flush();
    }
}

void Log_Write(uint8_t type, const char *text)
{
    LogMsg_t msg;
    uint8_t len;

    if (text == NULL || LogQueueHandle == NULL) return;

    len = (uint8_t)strlen(text);
    if (len >= LOG_TEXT_LEN) len = LOG_TEXT_LEN - 1U;

    msg.type = type;
    memset(msg.text, 0, sizeof(msg.text));
    memcpy(msg.text, text, len);
    (void)osMessageQueuePut(LogQueueHandle, &msg, 0U, 0U);
}

void Log_Task_Sys(void)
{
    LogMsg_t msg;
    uint32_t last_flush = HAL_GetTick();
    uint32_t last_header = HAL_GetTick();

    Log_Init();
    Log_Write(LOG_TYPE_BOOT, "BOOT OK");
    Log_Write(LOG_TYPE_SETTING, Set_Sys_LoadValid() ? "SET LOAD OK" : "SET LOAD FAIL");

    for (;;)
    {
        if (osMessageQueueGet(LogQueueHandle, &msg, NULL, 100U) == osOK)
        {
            Log_Append(&msg);
        }

        if ((HAL_GetTick() - last_flush) >= 1000U)
        {
            Log_Flush();
            last_flush = HAL_GetTick();
        }

        if ((HAL_GetTick() - last_header) >= 10000U)
        {
            Log_SaveHeader();
            last_header = HAL_GetTick();
        }
    }
}

static char Log_TypeChar(uint8_t type)
{
    switch (type)
    {
        case LOG_TYPE_BOOT:    return 'B';
        case LOG_TYPE_SETTING: return 'S';
        case LOG_TYPE_MUSIC:   return 'M';
        case LOG_TYPE_APP:     return 'A';
        case LOG_TYPE_ERROR:   return 'E';
        default:               return '?';
    }
}

static void Log_Render(uint8_t page, uint8_t pages)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("LOG ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);

    if (log_view_count == 0U)
    {
        OLED_SetCursor(0, 24);
        OLED_PrintString("No Log");
    }
    else
    {
        for (uint8_t row = 0U; row < 6U; row++)
        {
            uint8_t idx = (uint8_t)(page * 6U + row);
            if (idx >= log_view_count) break;

            OLED_SetCursor(0, (uint8_t)(8U + row * 8U));
            OLED_PrintChar(Log_TypeChar(log_view[idx].type));
            OLED_PrintChar(' ');
            OLED_PrintString(log_view[idx].text);
        }
    }

    OLED_SetCursor(0, 56);
    OLED_PrintString("4/6:PG *:BACK");
    OLED_Display();
}

void Log_View_Run(void)
{
    uint8_t page = 0U;
    char key;

    for (;;)
    {
        uint8_t pages = (log_view_count + 5U) / 6U;
        if (pages == 0U) pages = 1U;
        if (page >= pages) page = (uint8_t)(pages - 1U);

        Log_Render(page, pages);

        if (osMessageQueueGet(KeyHandle, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        if (key == '*') return;
        else if (key == '1') Music_Bg_Toggle();
        else if (key == '4')
        {
            if (page > 0U) page--;
        }
        else if (key == '6')
        {
            if (page + 1U < pages) page++;
        }
    }
}
