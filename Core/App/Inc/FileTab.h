/**
  ******************************************************************************
  * @file           : FileTab.h
  * @brief          : Persistent file table metadata on W25Q64
  ******************************************************************************
  */
#ifndef __FILE_TAB_H
#define __FILE_TAB_H

#include <stdint.h>

/* FLT1 occupies one reserved sector, see README W25Q64 partition table. */
#define FILE_TABLE_SECTOR_BASE    2U
#define FILE_TABLE_SECTOR_COUNT   1U

#define FILE_TABLE_MAGIC0  'F'
#define FILE_TABLE_MAGIC1  'L'
#define FILE_TABLE_MAGIC2  'T'
#define FILE_TABLE_MAGIC3  '1'

#define FILE_TABLE_VERSION  1U

#define FILE_TAB_KIND_PIC    0U
#define FILE_TAB_KIND_MUSIC  1U

typedef struct __attribute__((packed)) {
    uint8_t magic[4];
    uint16_t pic_sectors;      /* valid DRW1 sectors */
    uint16_t music_sectors;    /* valid MUS1 sectors */
    uint16_t managed_total;    /* DRW1 + MUS1 sector capacity */
    uint8_t version;
    uint8_t checksum;
} FileTable_t;

uint8_t FileTab_LoadValid(FileTable_t *tab);
uint8_t FileTab_Save(const FileTable_t *tab);
uint8_t FileTab_UpdateCounts(uint16_t pic_sectors, uint16_t music_sectors);
uint8_t FileTab_Refresh(uint8_t task_watch_id);
uint8_t FileTab_AdjustCount(uint8_t kind, int8_t delta, uint8_t task_watch_id);

#endif /* __FILE_TAB_H */
