/**
  ******************************************************************************
  * @file           : Log_App.c
  * @brief          : W25Q64 circular system log and viewer
  ******************************************************************************
  */

#include "Log_App.h"
#include "Confirm_App.h"
#include "Monitor_App.h"
#include "Screen_App.h"
#include "TaskWatch.h"
#include "TaskErr.h"
#include "Oled_App.h"
#include "CursorView.h"
#include "TaskSnap.h"
#include "UiInput.h"
#include "Music_App.h"
#include "Set_App.h"
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
#define LOG_RING_CAPACITY      (LOG_DATA_SECTORS * LOG_ENTRIES_PER_SECTOR)
#define LOG_QUEUE_DEPTH        8U
#define LOG_FLUSH_ENTRIES      8U
#define LOG_PAGE_ROWS          6U
#define LOG_TYPE_CLEAR         0xFFU
#define LOG_TYPE_FLUSH         0xFEU

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
static LogView_t log_page[LOG_PAGE_ROWS];
static uint8_t log_page_count;
static uint32_t log_total_count;
static volatile uint8_t log_clear_done;

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

static void Log_SaveHeader(void)
{
    LogHeader_t h;
    uint32_t addr = (uint32_t)LOG_HEADER_SECTOR * SFLASH_SECTOR_SIZE;

    h.magic = LOG_MAGIC;
    h.next_seq = log_next_seq;
    h.cur_sector = log_cur_sector;
    h.cur_offset = log_cur_offset;
    h.total = log_total_count;
    h.checksum = Log_HeaderChecksum(&h);

    if (SFlash_EraseSector(LOG_HEADER_SECTOR) == SFLASH_OK)
    {
        (void)SFlash_Write(addr, (const uint8_t *)&h, sizeof(h));
    }
}

static void Log_CountEntries(void)
{
    uint32_t count = 0U;

    for (uint32_t s = LOG_DATA_BASE; s < LOG_DATA_BASE + LOG_DATA_SECTORS; s++)
    {
        for (uint32_t i = 0U; i < LOG_ENTRIES_PER_SECTOR; i++)
        {
            LogEntry_t e;
            uint32_t addr = s * SFLASH_SECTOR_SIZE + i * LOG_ENTRY_SIZE;

            if (SFlash_Read(addr, (uint8_t *)&e, LOG_ENTRY_SIZE) == SFLASH_OK
             && Log_EntryValid(&e))
            {
                count++;
            }
        }
        TaskWatch_Beat(TASKWATCH_LOG);
        osDelay(1U);
    }
    log_total_count = count;
}

static void Log_RecoverTail(void)
{
    uint32_t offset = log_cur_offset;
    uint32_t next_seq = log_next_seq;
    uint32_t count = log_total_count;

    if (log_cur_sector < LOG_DATA_BASE
     || log_cur_sector >= LOG_DATA_BASE + LOG_DATA_SECTORS)
    {
        return;
    }

    while ((offset + LOG_ENTRY_SIZE) <= SFLASH_SECTOR_SIZE)
    {
        LogEntry_t e;
        uint32_t addr = log_cur_sector * SFLASH_SECTOR_SIZE + offset;

        if (SFlash_Read(addr, (uint8_t *)&e, LOG_ENTRY_SIZE) != SFLASH_OK)
        {
            break;
        }
        if (!Log_EntryValid(&e) || e.seq < next_seq)
        {
            break;
        }

        offset += LOG_ENTRY_SIZE;
        if (count < LOG_RING_CAPACITY) count++;
        if (e.seq >= next_seq) next_seq = e.seq + 1U;
    }

    log_cur_offset = offset;
    log_next_seq = next_seq;
    log_total_count = count;
}

static void Log_ReadPage(uint32_t page)
{
    uint32_t s = log_cur_sector;
    uint32_t limit = log_cur_offset / LOG_ENTRY_SIZE;
    uint32_t skip = page * LOG_PAGE_ROWS;
    uint32_t scanned = 0U;
    uint32_t budget = log_total_count;
    uint8_t got = 0U;

    if (log_total_count == 0U)
    {
        log_page_count = 0U;
        return;
    }

    if (limit > LOG_ENTRIES_PER_SECTOR) limit = LOG_ENTRIES_PER_SECTOR;

    while (scanned < LOG_DATA_SECTORS && got < LOG_PAGE_ROWS && budget > 0U)
    {
        for (uint32_t i = limit; i > 0U && got < LOG_PAGE_ROWS && budget > 0U; i--)
        {
            LogEntry_t e;
            uint32_t addr = s * SFLASH_SECTOR_SIZE + (i - 1U) * LOG_ENTRY_SIZE;

            budget--;

            /* 未刷入 Flash 或已擦除的槽位无效，跳过继续往前找 */
            if (SFlash_Read(addr, (uint8_t *)&e, LOG_ENTRY_SIZE) != SFLASH_OK
             || !Log_EntryValid(&e))
            {
                continue;
            }

            if (skip > 0U)
            {
                skip--;
                continue;
            }

            log_page[got].seq = e.seq;
            log_page[got].timestamp = e.timestamp;
            log_page[got].type = e.type;
            memset(log_page[got].text, 0, sizeof(log_page[got].text));
            memcpy(log_page[got].text, e.data, e.len);
            got++;
        }

        if (got >= LOG_PAGE_ROWS) break;

        s = (s <= LOG_DATA_BASE) ? LOG_DATA_BASE + LOG_DATA_SECTORS - 1U : s - 1U;
        limit = LOG_ENTRIES_PER_SECTOR;
        scanned++;
    }

    log_page_count = got;
}

static void Log_Scan(void)
{
    uint32_t max_seq = 0U;

    log_next_seq = 1U;
    log_cur_sector = LOG_DATA_BASE;
    log_cur_offset = 0U;
    log_total_count = 0U;

    for (uint32_t s = LOG_DATA_BASE; s < LOG_DATA_BASE + LOG_DATA_SECTORS; s++)
    {
        for (uint32_t i = 0U; i < LOG_ENTRIES_PER_SECTOR; i++)
        {
            LogEntry_t e;
            uint32_t addr = s * SFLASH_SECTOR_SIZE + i * LOG_ENTRY_SIZE;

            if (SFlash_Read(addr, (uint8_t *)&e, LOG_ENTRY_SIZE) != SFLASH_OK) continue;
            if (!Log_EntryValid(&e)) continue;

            log_total_count++;
            if (e.seq > max_seq)
            {
                max_seq = e.seq;
                log_cur_sector = s;
                log_cur_offset = (i + 1U) * LOG_ENTRY_SIZE;
            }
        }
        TaskWatch_Beat(TASKWATCH_LOG);
        osDelay(1U);
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
        LogEntry_t probe;
        uint32_t probe_addr;

        log_next_seq = h.next_seq;
        log_cur_sector = h.cur_sector;
        log_cur_offset = h.cur_offset;
        log_total_count = h.total;
        if (log_total_count == 0U && h.next_seq > 1U) Log_CountEntries();
        else if (log_total_count > 0U && log_cur_offset > 0U)
        {
            /* 头部指向的尾部无效说明上次写入/清空被中断，重新扫描重建状态 */
            probe_addr = log_cur_sector * SFLASH_SECTOR_SIZE
                       + log_cur_offset - LOG_ENTRY_SIZE;
            if (SFlash_Read(probe_addr, (uint8_t *)&probe, LOG_ENTRY_SIZE) != SFLASH_OK
             || !Log_EntryValid(&probe))
            {
                Log_Scan();
            }
        }
        Log_RecoverTail();
        Log_SaveHeader();
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

static void Log_EraseAll(void)
{
    uint8_t failed = 0U;
    uint8_t head[LOG_ENTRY_SIZE];
    uint8_t used;
    uint8_t i;

    /* 先把头部写成空日志，清空过程被中断也不会留下旧 total 导致下次全盘扫描 */
    log_flush_count = 0U;
    log_next_seq = 1U;
    log_cur_sector = LOG_DATA_BASE;
    log_cur_offset = 0U;
    log_total_count = 0U;
    log_ready = 1U;
    Log_SaveHeader();

    for (uint32_t s = LOG_DATA_BASE; s < LOG_DATA_BASE + LOG_DATA_SECTORS; s++)
    {
        used = 0U;
        if (SFlash_Read((uint32_t)s * SFLASH_SECTOR_SIZE, head, sizeof(head)) == SFLASH_OK)
        {
            for (i = 0U; i < sizeof(head); i++)
            {
                if (head[i] != 0xFFU)
                {
                    used = 1U;
                    break;
                }
            }
        }
        else
        {
            failed = 1U;
        }

        if (used != 0U && SFlash_EraseSector(s) != SFLASH_OK)
        {
            failed = 1U;
        }

        TaskWatch_Beat(TASKWATCH_LOG);
        osDelay(1U);
    }

    Log_SaveHeader();
    log_clear_done = 1U;

    if (failed != 0U)
    {
        Monitor_Sys_ReportError();
        Log_Write(LOG_TYPE_ERROR, "LOG CLEAR ERR");
    }
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
    if (log_total_count < LOG_RING_CAPACITY) log_total_count++;

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

void Log_FlushNow(void)
{
    LogMsg_t msg;

    if (LogQueueHandle == NULL) return;

    msg.type = LOG_TYPE_FLUSH;
    memset(msg.text, 0, sizeof(msg.text));
    (void)osMessageQueuePut(LogQueueHandle, &msg, 0U, 1000U);
}

uint8_t Log_Clear(void)
{
    LogMsg_t msg;

    if (LogQueueHandle == NULL) return 0U;

    log_clear_done = 0U;
    msg.type = LOG_TYPE_CLEAR;
    memset(msg.text, 0, sizeof(msg.text));

    if (osMessageQueuePut(LogQueueHandle, &msg, 0U, 1000U) != osOK)
    {
        log_clear_done = 1U;
        return 0U;
    }

    return 1U;
}

uint8_t Log_IsClearing(void)
{
    return (log_clear_done == 0U) ? 1U : 0U;
}

void Log_Task_Sys(void)
{
    LogMsg_t msg;
    uint32_t last_flush = HAL_GetTick();
    uint32_t last_header = HAL_GetTick();

    Log_Init();
    TaskErr_Init();
    Log_Write(LOG_TYPE_BOOT, "BOOT OK");
    Log_Write(LOG_TYPE_SETTING, Set_Sys_LoadValid() ? "SET LOAD OK" : "SET LOAD FAIL");

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_LOG);

        if (osMessageQueueGet(LogQueueHandle, &msg, NULL, 100U) == osOK)
        {
            if (msg.type == LOG_TYPE_CLEAR)
            {
                Log_EraseAll();
            }
            else if (msg.type == LOG_TYPE_FLUSH)
            {
                Log_Flush();
                Log_SaveHeader();
            }
            else
            {
                Log_Append(&msg);
            }
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
        case LOG_TYPE_DRAW:    return 'D';
        default:               return '?';
    }
}

static uint8_t Log_HitLeftPage(const CursorMsg_t *msg)
{
    return (msg->cursor_x >= 0 && msg->cursor_x < 40
         && msg->cursor_y >= 56 && msg->cursor_y < OLED_HEIGHT) ? 1U : 0U;
}

static uint8_t Log_HitRightPage(const CursorMsg_t *msg)
{
    return (msg->cursor_x >= 88 && msg->cursor_x < OLED_WIDTH
         && msg->cursor_y >= 56 && msg->cursor_y < OLED_HEIGHT) ? 1U : 0U;
}

static void Log_Render(uint16_t page, uint16_t pages)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("LOG ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);

    if (log_total_count == 0U)
    {
        OLED_SetCursor(0, 24);
        OLED_PrintString("No Log");
    }
    else
    {
        for (uint8_t row = 0U; row < log_page_count; row++)
        {
            OLED_SetCursor(0, (uint8_t)(8U + row * 8U));
            OLED_PrintChar(Log_TypeChar(log_page[row].type));
            OLED_PrintChar(' ');
            OLED_PrintString(log_page[row].text);
        }
    }

    OLED_Display();
}

static void Log_DrawHintRow(uint8_t left_sel, uint8_t right_sel)
{
    CursorView_Erase();

    OLED_FillRect(0, 56U, OLED_WIDTH, 8U, OLED_BLACK);

    if (left_sel != 0U)
    {
        OLED_SetCursor(0, 56);
        OLED_PrintString("[<<]");
    }
    else
    {
        OLED_SetCursor(0, 56);
        OLED_PrintString("<<");
    }

    if (right_sel != 0U)
    {
        OLED_SetCursor(104, 56);
        OLED_PrintString("[>>]");
    }
    else
    {
        OLED_SetCursor(116, 56);
        OLED_PrintString(">>");
    }

    OLED_UpdateRect(0, 56U, OLED_WIDTH, 8U);
    CursorView_Place();
}

static void Log_LogViewer_Run(void)
{
    uint16_t page = 0U;
    uint16_t pages = 1U;
    char key;
    uint8_t clearing = 0U;
    uint8_t need_render = 1U;
    uint8_t prev_joy_button = 0U;
    uint8_t left_sel = 0U;
    uint8_t right_sel = 0U;
    CursorMsg_t cur = {64, 32, 0};

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_OLED);

        Cursor_GetPos(&cur.cursor_x, &cur.cursor_y);

        if (clearing != 0U)
        {
            if (Log_IsClearing())
            {
                TaskWatch_Beat(TASKWATCH_OLED);

                OLED_Clear();
                OLED_SetCursor(0, 8);
                OLED_PrintString("Clearing...");
                OLED_Display();
                Screen_Sys_Wake();
                osDelay(100U);
                continue;
            }

            clearing = 0U;
            page = 0U;
            need_render = 1U;
            OLED_Clear();
            OLED_SetCursor(16, 28);
            OLED_PrintString("CLEARED");
            OLED_Display();
            osDelay(300U);
        }

        if (need_render != 0U)
        {
            pages = (uint16_t)((log_total_count + LOG_PAGE_ROWS - 1U)
                               / LOG_PAGE_ROWS);
            if (pages == 0U) pages = 1U;
            if (page >= pages) page = (uint16_t)(pages - 1U);

            Log_ReadPage((uint32_t)page);
            Log_Render(page, pages);
        }

        {
            uint8_t new_left = Log_HitLeftPage(&cur);
            uint8_t new_right = Log_HitRightPage(&cur);

            if (need_render != 0U
             || new_left != left_sel
             || new_right != right_sel)
            {
                left_sel = new_left;
                right_sel = new_right;
                Log_DrawHintRow(left_sel, right_sel);
            }
            need_render = 0U;
        }

        while (osMessageQueueGet(cursorHandle, &cur, NULL, 0U) == osOK)
        {
            if (cur.button_pressed != 0U && prev_joy_button == 0U)
            {
                if (Log_HitLeftPage(&cur) && page > 0U)
                {
                    page--;
                    need_render = 1U;
                }
                else if (Log_HitRightPage(&cur) && page + 1U < pages)
                {
                    page++;
                    need_render = 1U;
                }
            }
            prev_joy_button = cur.button_pressed;
        }

        if (osMessageQueueGet(KeyHandle, &key, NULL, 0U) == osOK)
        {
            if (key == '*') return;
            else if (key == '1') Music_Bg_Toggle();
            else if (key == '0')
            {
                if (Confirm_Ask("Clear Logs") && Log_Clear())
                {
                    clearing = 1U;
                }
            }
            else if (key == '#')
            {
                if (Log_HitLeftPage(&cur) && page > 0U)
                {
                    page--;
                    need_render = 1U;
                }
                else if (Log_HitRightPage(&cur) && page + 1U < pages)
                {
                    page++;
                    need_render = 1U;
                }
            }
            else if (key == '4')
            {
                if (page > 0U) page--;
                need_render = 1U;
            }
            else if (key == '6')
            {
                if (page + 1U < pages) page++;
                need_render = 1U;
            }
        }

        CursorView_Track();
        osDelay(10U);
    }
}

static void Log_MenuRender(uint8_t sel)
{
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("LOG");
    OLED_SetCursor(0, 16);
    OLED_PrintString(sel == 0U ? "> LOG VIEW" : "  LOG VIEW");
    OLED_SetCursor(0, 24);
    OLED_PrintString(sel == 1U ? "> TASK VIEW" : "  TASK VIEW");
    OLED_Display();
}

static void Log_TaskViewRender(const TaskSnapHeader_t *header,
                               uint8_t entry_count, uint8_t page,
                               uint8_t snap_ord, uint8_t snap_count)
{
    uint8_t pages = (uint8_t)(1U + (entry_count + LOG_PAGE_ROWS - 1U)
                                  / LOG_PAGE_ROWS);
    uint8_t start;
    uint8_t i;

    if (page >= pages) page = (uint8_t)(pages - 1U);

    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_PrintString("TSK ");
    OLED_PrintNum((uint32_t)snap_ord + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)snap_count, 10);
    OLED_PrintString(" #");
    OLED_PrintNum(header->seq, 10);

    if (page == 0U)
    {
        TaskSnapSummary_t summary;

        if (TaskSnap_GetSummary(snap_ord, &summary))
        {
            OLED_SetCursor(0, 8);
            OLED_PrintString("HEAP ");
            OLED_PrintNum(summary.heap_free, 10);

            OLED_SetCursor(0, 16);
            OLED_PrintString("ERR ");
            OLED_PrintNum(summary.error_count, 10);

            OLED_SetCursor(0, 24);
            OLED_PrintString("EV ");
            OLED_PrintNum(summary.event_count, 10);
            OLED_PrintString(" DRP ");
            OLED_PrintNum(summary.dropped_count, 10);

            OLED_SetCursor(0, 32);
            OLED_PrintString("CUR ");
            OLED_PrintNum((uint32_t)summary.cur_queue, 10);
            OLED_PrintString(" KEY ");
            OLED_PrintNum((uint32_t)summary.key_queue, 10);

            OLED_SetCursor(0, 40);
            OLED_PrintString("LOG ");
            OLED_PrintNum((uint32_t)summary.log_queue, 10);
        }
    }
    else
    {
        start = (uint8_t)((page - 1U) * LOG_PAGE_ROWS);

        for (i = 0U; i < LOG_PAGE_ROWS; i++)
        {
            uint8_t idx = (uint8_t)(start + i);

            if (idx >= entry_count) break;

            OLED_SetCursor(0, (uint8_t)(8U + i * 8U));
            {
                TaskSnapEntry_t entry;

                if (TaskSnap_GetEntry(snap_ord, idx, &entry))
                {
                    OLED_PrintString(entry.name);
                    OLED_PrintChar(' ');
                    OLED_PrintChar((char)entry.state);
                    OLED_PrintChar(' ');
                    OLED_PrintNum((uint32_t)entry.stack_free, 10);
                }
            }
        }
    }

    OLED_SetCursor(0, 56);
    OLED_PrintString("< ");
    OLED_PrintNum((uint32_t)page + 1U, 10);
    OLED_PrintChar('/');
    OLED_PrintNum((uint32_t)pages, 10);
    OLED_PrintString(" >");
    OLED_Display();
}

static void Log_TaskViewer_Run(void)
{
    uint8_t snap_count = 0U;
    uint8_t snap_ord = 0U;
    uint8_t page = 0U;
    uint8_t need_render = 1U;
    uint8_t start_at_latest = 1U;
    char key;

    for (;;)
    {
        TaskSnapHeader_t header;
        uint8_t entry_count = 0U;
        uint8_t pages;
        CursorMsg_t drop;

        TaskWatch_Beat(TASKWATCH_OLED);
        while (osMessageQueueGet(cursorHandle, &drop, NULL, 0U) == osOK) { }

        snap_count = TaskSnap_GetValidCount();
        if (snap_count == 0U)
        {
            if (need_render != 0U)
            {
                OLED_Clear();
                OLED_SetCursor(0, 8);
                OLED_PrintString("No Task Log");
                OLED_SetCursor(0, 24);
                OLED_PrintString("Press B to save");
                OLED_Display();
                need_render = 0U;
            }

            if (Ui_KeyGet(&key, 100U) != osOK) continue;
            if (key == '*') return;
            if (key == '1') Music_Bg_Toggle();
            need_render = 1U;
            continue;
        }

        if (start_at_latest != 0U)
        {
            snap_ord = (uint8_t)(snap_count - 1U);
            start_at_latest = 0U;
        }
        if (snap_ord >= snap_count) snap_ord = (uint8_t)(snap_count - 1U);

        if (!TaskSnap_GetSnapshotHeader(snap_ord, &header))
        {
            snap_count = 0U;
            need_render = 1U;
            continue;
        }
        entry_count = header.count;

        pages = (uint8_t)(1U + (entry_count + LOG_PAGE_ROWS - 1U)
                              / LOG_PAGE_ROWS);
        if (page >= pages) page = (uint8_t)(pages - 1U);

        if (need_render != 0U)
        {
            Log_TaskViewRender(&header, entry_count, page, snap_ord, snap_count);
            need_render = 0U;
        }

        if (Ui_KeyGet(&key, 100U) != osOK) continue;

        if (key == '*') return;
        else if (key == '1') Music_Bg_Toggle();
        else if (key == '0')
        {
            if (TaskSnap_Delete(snap_ord))
            {
                OLED_Clear();
                OLED_SetCursor(16, 28);
                OLED_PrintString("DELETED");
                OLED_Display();
                osDelay(300U);

                if (snap_ord + 1U >= snap_count && snap_ord > 0U)
                {
                    snap_ord--;
                }
                page = 0U;
                need_render = 1U;
            }
        }
        else if (key == '2')
        {
            if (page > 0U) page--;
            need_render = 1U;
        }
        else if (key == '8')
        {
            if (page + 1U < pages) page++;
            need_render = 1U;
        }
        else if (key == '4')
        {
            if (snap_ord > 0U)
            {
                snap_ord--;
                page = 0U;
                need_render = 1U;
            }
        }
        else if (key == '6')
        {
            if (snap_ord + 1U < snap_count)
            {
                snap_ord++;
                page = 0U;
                need_render = 1U;
            }
        }
        else
        {
            need_render = 1U;
        }
    }
}

void Log_View_Run(void)
{
    uint8_t sel = 0U;
    char key;
    CursorMsg_t drop;

    for (;;)
    {
        TaskWatch_Beat(TASKWATCH_OLED);
        while (osMessageQueueGet(cursorHandle, &drop, NULL, 0U) == osOK) { }

        Log_MenuRender(sel);

        if (Ui_KeyGet(&key, 100U) != osOK) continue;

        if (key == '*') return;
        else if (key == '1') Music_Bg_Toggle();
        else if (key == '2' || key == '8') sel ^= 1U;
        else if (key == '#')
        {
            if (sel == 0U) Log_LogViewer_Run();
            else Log_TaskViewer_Run();
        }
    }
}
