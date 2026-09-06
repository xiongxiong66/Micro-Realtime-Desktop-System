/**
  ******************************************************************************
  * @file           : FileTab.c
  * @brief          : Persistent file table metadata on W25Q64
  ******************************************************************************
  */

#include "FileTab.h"
#include "ImgFile.h"
#include "MusicFile.h"
#include "sflash.h"
#include "TaskWatch.h"
#include <string.h>

#define FILE_TABLE_MANAGED_SECTORS \
    ((uint16_t)(DRAW_SECTOR_COUNT + MUSIC_SECTOR_COUNT))

static uint8_t FileTab_Checksum(const FileTable_t *tab)
{
    const uint8_t *p = (const uint8_t *)tab;
    uint8_t sum = 0U;
    uint16_t i;

    if (tab == NULL) return 0U;

    for (i = 0U; i < sizeof(FileTable_t) - 1U; i++)
    {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

uint8_t FileTab_LoadValid(FileTable_t *tab)
{
    uint32_t used;

    if (tab == NULL) return 0U;

    if (SFlash_LoadSector(FILE_TABLE_SECTOR_BASE,
                          (uint8_t *)tab, sizeof(FileTable_t)) != SFLASH_OK)
    {
        return 0U;
    }

    if (tab->magic[0] != FILE_TABLE_MAGIC0
     || tab->magic[1] != FILE_TABLE_MAGIC1
     || tab->magic[2] != FILE_TABLE_MAGIC2
     || tab->magic[3] != FILE_TABLE_MAGIC3) return 0U;

    if (tab->version != FILE_TABLE_VERSION) return 0U;
    if (tab->managed_total != FILE_TABLE_MANAGED_SECTORS) return 0U;
    if (tab->pic_sectors > DRAW_SECTOR_COUNT) return 0U;
    if (tab->music_sectors > MUSIC_SECTOR_COUNT) return 0U;

    used = (uint32_t)tab->pic_sectors + (uint32_t)tab->music_sectors;
    if (used > (uint32_t)tab->managed_total) return 0U;

    return (tab->checksum == FileTab_Checksum(tab)) ? 1U : 0U;
}

uint8_t FileTab_Save(const FileTable_t *tab)
{
    FileTable_t out;

    if (tab == NULL) return 0U;

    out = *tab;
    out.checksum = FileTab_Checksum(&out);

    return (SFlash_SaveSector(FILE_TABLE_SECTOR_BASE,
                              (const uint8_t *)&out,
                              sizeof(FileTable_t)) == SFLASH_OK) ? 1U : 0U;
}

uint8_t FileTab_UpdateCounts(uint16_t pic_sectors, uint16_t music_sectors)
{
    FileTable_t tab;

    if (FileTab_LoadValid(&tab)
     && tab.pic_sectors == pic_sectors
     && tab.music_sectors == music_sectors)
    {
        return 1U;
    }

    memset(&tab, 0, sizeof(tab));
    tab.magic[0] = FILE_TABLE_MAGIC0;
    tab.magic[1] = FILE_TABLE_MAGIC1;
    tab.magic[2] = FILE_TABLE_MAGIC2;
    tab.magic[3] = FILE_TABLE_MAGIC3;
    tab.pic_sectors = pic_sectors;
    tab.music_sectors = music_sectors;
    tab.managed_total = FILE_TABLE_MANAGED_SECTORS;
    tab.version = FILE_TABLE_VERSION;

    if (!FileTab_Save(&tab)) return 0U;
    return FileTab_LoadValid(&tab);
}

uint8_t FileTab_Refresh(uint8_t task_watch_id)
{
    uint16_t pic_count = 0U;
    uint16_t music_count = 0U;
    uint16_t i;

    for (i = 0U; i < DRAW_SECTOR_COUNT; i++)
    {
        if (DrawFile_MagicValid((uint16_t)(DRAW_SECTOR_BASE + i)))
        {
            pic_count++;
        }
        if ((i & 0x1FU) == 0x1FU)
        {
            TaskWatch_Beat(task_watch_id);
        }
    }

    for (i = 0U; i < MUSIC_SECTOR_COUNT; i++)
    {
        if (MusicFile_MagicValid((uint16_t)(MUSIC_SECTOR_BASE + i)))
        {
            music_count++;
        }
        if ((i & 0x3FU) == 0x3FU)
        {
            TaskWatch_Beat(task_watch_id);
        }
    }
    TaskWatch_Beat(task_watch_id);

    return FileTab_UpdateCounts(pic_count, music_count);
}

uint8_t FileTab_AdjustCount(uint8_t kind, int8_t delta, uint8_t task_watch_id)
{
    FileTable_t tab;
    uint16_t new_count;
    uint16_t limit;

    if (kind != FILE_TAB_KIND_PIC && kind != FILE_TAB_KIND_MUSIC) return 0U;

    if (!FileTab_LoadValid(&tab))
    {
        return FileTab_Refresh(task_watch_id);
    }

    new_count = (kind == FILE_TAB_KIND_PIC) ? tab.pic_sectors
                                            : tab.music_sectors;
    limit = (kind == FILE_TAB_KIND_PIC) ? DRAW_SECTOR_COUNT
                                        : MUSIC_SECTOR_COUNT;

    if (delta > 0)
    {
        if (new_count >= limit) return FileTab_Refresh(task_watch_id);
        new_count++;
    }
    else if (delta < 0)
    {
        if (new_count == 0U) return FileTab_Refresh(task_watch_id);
        new_count--;
    }
    else
    {
        return 1U;
    }

    if (kind == FILE_TAB_KIND_PIC) tab.pic_sectors = new_count;
    else tab.music_sectors = new_count;

    if (!FileTab_Save(&tab)) return 0U;
    return FileTab_LoadValid(&tab);
}
