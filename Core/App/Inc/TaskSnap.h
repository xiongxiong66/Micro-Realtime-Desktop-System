/**
  ******************************************************************************
  * @file           : TaskSnap.h
  * @brief          : Task state snapshots stored in reserved W25Q64 sectors
  ******************************************************************************
  */
#ifndef __TASK_SNAP_H
#define __TASK_SNAP_H

#include <stdint.h>

#define TASK_SNAP_SECTOR_BASE   3U
#define TASK_SNAP_SECTOR_COUNT  8U

#define TASK_SNAP_MAGIC0  'T'
#define TASK_SNAP_MAGIC1  'K'
#define TASK_SNAP_MAGIC2  'S'
#define TASK_SNAP_MAGIC3  '1'

#define TASK_SNAP_VERSION  2U

#define TASK_SNAP_MAX_TASKS   16U
#define TASK_SNAP_NAME_LEN    12U
#define TASK_SNAP_HEADER_SIZE 16U
#define TASK_SNAP_SUMMARY_SIZE 24U
#define TASK_SNAP_ENTRY_SIZE  16U

typedef struct __attribute__((packed)) {
    uint8_t magic[4];
    uint32_t seq;
    uint32_t timestamp;
    uint8_t count;
    uint8_t version;
    uint8_t reserved;
    uint8_t checksum;
} TaskSnapHeader_t;

typedef struct __attribute__((packed)) {
    char name[TASK_SNAP_NAME_LEN];
    uint8_t state;      /* ASCII state char: R/r/B/S/D */
    uint8_t priority;
    uint16_t stack_free;/* bytes */
} TaskSnapEntry_t;

typedef struct __attribute__((packed)) {
    uint32_t heap_free;
    uint32_t error_count;
    uint32_t event_count;
    uint32_t dropped_count;
    uint16_t cur_queue;
    uint16_t key_queue;
    uint16_t log_queue;
    uint16_t reserved;
} TaskSnapSummary_t;

uint8_t TaskSnap_Record(void);
uint8_t TaskSnap_GetValidCount(void);
uint8_t TaskSnap_GetSnapshotHeader(uint8_t ordinal, TaskSnapHeader_t *header);
uint8_t TaskSnap_GetSummary(uint8_t ordinal, TaskSnapSummary_t *summary);
uint8_t TaskSnap_GetEntry(uint8_t ordinal, uint8_t index,
                          TaskSnapEntry_t *entry);

#endif /* __TASK_SNAP_H */
