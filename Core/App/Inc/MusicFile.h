/**
  ******************************************************************************
  * @file           : MusicFile.h
  * @brief          : Shared music file format stored in W25Q64
  ******************************************************************************
  */
#ifndef __MUSIC_FILE_H
#define __MUSIC_FILE_H

#include <stdint.h>

/* Music region, see README W25Q64 partition table */
#define MUSIC_SECTOR_BASE   544U
#define MUSIC_SECTOR_COUNT  256U
#define MUSIC_NAME_LEN      12U

#define MUSIC_MAGIC0  'M'
#define MUSIC_MAGIC1  'U'
#define MUSIC_MAGIC2  'S'
#define MUSIC_MAGIC3  '1'

/* magic(4) + name(12) + event_count(2) + checksum(1) */
#define MUSIC_HEADER_BYTES  19U

typedef struct {
    uint16_t frequency_hz;   /* 0 means a rest */
    uint16_t duration_ms;
} MusicEvent_t;

typedef struct {
    uint8_t magic[4];
    char name[MUSIC_NAME_LEN];
    uint16_t event_count;
    uint8_t checksum;
    MusicEvent_t events[];
} MusicFile_t;

uint8_t MusicFile_HeaderValid(uint16_t sector, uint8_t hdr[MUSIC_HEADER_BYTES]);
uint8_t MusicFile_MagicValid(uint16_t sector);
uint8_t MusicFile_ComputeChecksum(uint16_t sector, uint8_t hdr[MUSIC_HEADER_BYTES]);

#endif /* __MUSIC_FILE_H */
