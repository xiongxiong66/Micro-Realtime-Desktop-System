/**
  ******************************************************************************
  * @file           : TaskSnap.c
  * @brief          : Task state snapshots stored in reserved W25Q64 sectors
  ******************************************************************************
  */

#include "TaskSnap.h"
#include "sflash.h"
#include "Monitor_App.h"
#include "Oled_App.h"
#include "MKey_App.h"
#include "Log_App.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <string.h>

static TaskStatus_t task_snap_status[TASK_SNAP_MAX_TASKS];

static uint8_t TaskSnap_HeaderChecksum(const TaskSnapHeader_t *header)
{
    const uint8_t *p = (const uint8_t *)header;
    uint8_t sum = 0U;
    uint16_t i;

    if (header == NULL) return 0U;

    for (i = 0U; i < sizeof(TaskSnapHeader_t) - 1U; i++)
    {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t TaskSnap_HeaderValid(const TaskSnapHeader_t *header)
{
    if (header == NULL) return 0U;

    if (header->magic[0] != TASK_SNAP_MAGIC0
     || header->magic[1] != TASK_SNAP_MAGIC1
     || header->magic[2] != TASK_SNAP_MAGIC2
     || header->magic[3] != TASK_SNAP_MAGIC3) return 0U;

    if (header->version != TASK_SNAP_VERSION) return 0U;
    if (header->count > TASK_SNAP_MAX_TASKS) return 0U;
    if (header->seq == 0U) return 0U;

    return (header->checksum == TaskSnap_HeaderChecksum(header)) ? 1U : 0U;
}

static uint8_t TaskSnap_ReadHeader(uint8_t sector, TaskSnapHeader_t *header)
{
    uint32_t addr = (uint32_t)(TASK_SNAP_SECTOR_BASE + sector) * SFLASH_SECTOR_SIZE;

    if (header == NULL) return 0U;
    if (SFlash_Read(addr, (uint8_t *)header, sizeof(TaskSnapHeader_t)) != SFLASH_OK)
    {
        return 0U;
    }
    return TaskSnap_HeaderValid(header);
}

static uint8_t TaskSnap_StateChar(eTaskState state)
{
    switch (state)
    {
        case eRunning:   return 'R';
        case eReady:     return 'r';
        case eBlocked:   return 'B';
        case eSuspended: return 'S';
        case eDeleted:   return 'D';
        default:         return '?';
    }
}

uint8_t TaskSnap_Record(void)
{
    UBaseType_t total = uxTaskGetNumberOfTasks();
    UBaseType_t got;
    TaskSnapHeader_t header;
    TaskSnapSummary_t summary;
    uint32_t max_seq = 0U;
    uint8_t max_sector = 0U;
    uint8_t has_valid = 0U;
    uint8_t free_sector = 0xFFU;
    uint8_t target;
    uint8_t i;
    uint8_t count = 0U;
    uint32_t addr;
    uint32_t entry_addr;
    uint32_t new_seq;

    if (total > TASK_SNAP_MAX_TASKS) total = TASK_SNAP_MAX_TASKS;
    if (uxTaskGetSystemState(task_snap_status, total, NULL) != total) return 0U;

    for (i = 0U; i < TASK_SNAP_SECTOR_COUNT; i++)
    {
        if (TaskSnap_ReadHeader(i, &header))
        {
            if (!has_valid || header.seq > max_seq)
            {
                max_seq = header.seq;
                max_sector = i;
            }
            has_valid = 1U;
        }
        else if (free_sector == 0xFFU)
        {
            free_sector = i;
        }
    }

    if (free_sector != 0xFFU) target = free_sector;
    else target = (uint8_t)((max_sector + 1U) % TASK_SNAP_SECTOR_COUNT);

    addr = (uint32_t)(TASK_SNAP_SECTOR_BASE + target) * SFLASH_SECTOR_SIZE;
    if (SFlash_EraseSector(TASK_SNAP_SECTOR_BASE + target) != SFLASH_OK) return 0U;

    memset(&summary, 0, sizeof(summary));
    summary.heap_free = (uint32_t)xPortGetFreeHeapSize();
    summary.error_count = Monitor_Sys_GetErrorCount();
    summary.event_count = adc_events + mkey_events;
    summary.dropped_count = adc_dropped + mkey_dropped;
    summary.cur_queue = (cursorHandle != NULL)
                      ? (uint16_t)osMessageQueueGetCount(cursorHandle) : 0U;
    summary.key_queue = (KeyHandle != NULL)
                      ? (uint16_t)osMessageQueueGetCount(KeyHandle) : 0U;
    summary.log_queue = (LogQueueHandle != NULL)
                      ? (uint16_t)osMessageQueueGetCount(LogQueueHandle) : 0U;

    if (SFlash_Write(addr + TASK_SNAP_HEADER_SIZE,
                     (const uint8_t *)&summary,
                     sizeof(summary)) != SFLASH_OK)
    {
        return 0U;
    }

    entry_addr = addr + TASK_SNAP_HEADER_SIZE + TASK_SNAP_SUMMARY_SIZE;
    for (got = 0U; got < total; got++)
    {
        TaskSnapEntry_t entry;
        eTaskState st = task_snap_status[got].eCurrentState;

        if (st == eSuspended || st == eDeleted) continue;

        memset(&entry, 0, sizeof(entry));
        if (task_snap_status[got].pcTaskName != NULL)
        {
            strncpy(entry.name, task_snap_status[got].pcTaskName,
                    sizeof(entry.name) - 1U);
            entry.name[sizeof(entry.name) - 1U] = '\0';
        }
        entry.state = TaskSnap_StateChar(st);
        entry.priority = (uint8_t)task_snap_status[got].uxCurrentPriority;
        entry.stack_free = (uint16_t)(task_snap_status[got].usStackHighWaterMark
                                      * sizeof(StackType_t));

        if (SFlash_Write(entry_addr, (const uint8_t *)&entry,
                         sizeof(entry)) != SFLASH_OK)
        {
            return 0U;
        }
        entry_addr += sizeof(entry);
        count++;
    }

    new_seq = has_valid ? (max_seq + 1U) : 1U;

    memset(&header, 0, sizeof(header));
    header.magic[0] = TASK_SNAP_MAGIC0;
    header.magic[1] = TASK_SNAP_MAGIC1;
    header.magic[2] = TASK_SNAP_MAGIC2;
    header.magic[3] = TASK_SNAP_MAGIC3;
    header.seq = (uint32_t)new_seq;
    header.timestamp = HAL_GetTick() / 1000U;
    header.count = count;
    header.version = TASK_SNAP_VERSION;
    header.checksum = TaskSnap_HeaderChecksum(&header);

    return (SFlash_Write(addr, (const uint8_t *)&header,
                         sizeof(header)) == SFLASH_OK) ? 1U : 0U;
}

uint8_t TaskSnap_GetValidCount(void)
{
    TaskSnapHeader_t header;
    uint8_t count = 0U;
    uint8_t i;

    for (i = 0U; i < TASK_SNAP_SECTOR_COUNT; i++)
    {
        if (TaskSnap_ReadHeader(i, &header)) count++;
    }
    return count;
}

static uint8_t TaskSnap_SelectSector(uint8_t ordinal)
{
    uint32_t seqs[TASK_SNAP_SECTOR_COUNT];
    uint8_t sectors[TASK_SNAP_SECTOR_COUNT];
    uint8_t valid = 0U;
    uint8_t i;
    uint8_t j;
    TaskSnapHeader_t tmp;

    for (i = 0U; i < TASK_SNAP_SECTOR_COUNT; i++)
    {
        if (TaskSnap_ReadHeader(i, &tmp))
        {
            sectors[valid] = i;
            seqs[valid] = tmp.seq;
            valid++;
        }
    }

    if (ordinal >= valid) return 0xFFU;

    for (i = 0U; i + 1U < valid; i++)
    {
        for (j = 0U; j + 1U < valid - i; j++)
        {
            if (seqs[j] > seqs[j + 1U])
            {
                uint32_t tseq = seqs[j];
                uint8_t tsec = sectors[j];
                seqs[j] = seqs[j + 1U];
                sectors[j] = sectors[j + 1U];
                seqs[j + 1U] = tseq;
                sectors[j + 1U] = tsec;
            }
        }
    }

    return sectors[ordinal];
}

uint8_t TaskSnap_GetSnapshotHeader(uint8_t ordinal, TaskSnapHeader_t *header)
{
    uint8_t sector;

    if (header == NULL) return 0U;
    sector = TaskSnap_SelectSector(ordinal);
    if (sector == 0xFFU) return 0U;

    return TaskSnap_ReadHeader(sector, header);
}

uint8_t TaskSnap_GetSummary(uint8_t ordinal, TaskSnapSummary_t *summary)
{
    uint8_t sector;
    uint32_t addr;

    if (summary == NULL) return 0U;
    sector = TaskSnap_SelectSector(ordinal);
    if (sector == 0xFFU) return 0U;

    addr = (uint32_t)(TASK_SNAP_SECTOR_BASE + sector) * SFLASH_SECTOR_SIZE
         + TASK_SNAP_HEADER_SIZE;
    return (SFlash_Read(addr, (uint8_t *)summary,
                        sizeof(TaskSnapSummary_t)) == SFLASH_OK) ? 1U : 0U;
}

uint8_t TaskSnap_GetEntry(uint8_t ordinal, uint8_t index,
                          TaskSnapEntry_t *entry)
{
    uint8_t sector;
    TaskSnapHeader_t header;
    uint32_t addr;

    if (entry == NULL) return 0U;
    sector = TaskSnap_SelectSector(ordinal);
    if (sector == 0xFFU) return 0U;
    if (!TaskSnap_ReadHeader(sector, &header)) return 0U;
    if (index >= header.count) return 0U;

    addr = (uint32_t)(TASK_SNAP_SECTOR_BASE + sector) * SFLASH_SECTOR_SIZE
         + TASK_SNAP_HEADER_SIZE + TASK_SNAP_SUMMARY_SIZE
         + (uint32_t)index * TASK_SNAP_ENTRY_SIZE;
    return (SFlash_Read(addr, (uint8_t *)entry,
                        TASK_SNAP_ENTRY_SIZE) == SFLASH_OK) ? 1U : 0U;
}
