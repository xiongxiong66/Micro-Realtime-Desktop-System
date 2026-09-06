/**
  ******************************************************************************
  * @file           : MusicFile.c
  * @brief          : Shared music file checksum helpers
  ******************************************************************************
  */

#include "MusicFile.h"
#include "sflash.h"
#include <stddef.h>

#define MUSIC_CHECK_CHUNK 64U

uint8_t MusicFile_MagicValid(uint16_t sector)
{
    uint8_t magic[4];

    if (SFlash_Read((uint32_t)sector * SFLASH_SECTOR_SIZE,
                    magic, sizeof(magic)) != SFLASH_OK) return 0U;

    return (magic[0] == MUSIC_MAGIC0
         && magic[1] == MUSIC_MAGIC1
         && magic[2] == MUSIC_MAGIC2
         && magic[3] == MUSIC_MAGIC3) ? 1U : 0U;
}

uint8_t MusicFile_HeaderValid(uint16_t sector, uint8_t hdr[MUSIC_HEADER_BYTES])
{
    uint8_t chunk[MUSIC_CHECK_CHUNK];
    uint8_t sum = 0U;
    uint32_t addr;
    uint32_t remain;
    uint16_t count;
    uint8_t i;

    if (hdr == NULL) return 0U;

    addr = (uint32_t)sector * SFLASH_SECTOR_SIZE;
    if (SFlash_Read(addr, hdr, MUSIC_HEADER_BYTES) != SFLASH_OK) return 0U;

    if (hdr[0] != MUSIC_MAGIC0 || hdr[1] != MUSIC_MAGIC1
     || hdr[2] != MUSIC_MAGIC2 || hdr[3] != MUSIC_MAGIC3) return 0U;

    count = (uint16_t)((uint16_t)hdr[16U] | ((uint16_t)hdr[17U] << 8U));
    if (count > (SFLASH_SECTOR_SIZE - MUSIC_HEADER_BYTES) / sizeof(MusicEvent_t)) return 0U;

    for (i = 0U; i < 18U; i++)
    {
        sum = (uint8_t)(sum + hdr[i]);
    }

    addr += MUSIC_HEADER_BYTES;
    remain = (uint32_t)count * sizeof(MusicEvent_t);

    while (remain > 0U)
    {
        uint32_t n = (remain > MUSIC_CHECK_CHUNK) ? MUSIC_CHECK_CHUNK : remain;

        if (SFlash_Read(addr, chunk, n) != SFLASH_OK) return 0U;
        for (i = 0U; i < n; i++)
        {
            sum = (uint8_t)(sum + chunk[i]);
        }
        addr += n;
        remain -= n;
    }

    return (hdr[18U] == sum) ? 1U : 0U;
}

uint8_t MusicFile_ComputeChecksum(uint16_t sector, uint8_t hdr[MUSIC_HEADER_BYTES])
{
    uint8_t chunk[MUSIC_CHECK_CHUNK];
    uint8_t sum = 0U;
    uint32_t addr;
    uint32_t remain;
    uint16_t count;
    uint8_t i;

    if (hdr == NULL) return 0U;

    count = (uint16_t)((uint16_t)hdr[16U] | ((uint16_t)hdr[17U] << 8U));
    if (count > (SFLASH_SECTOR_SIZE - MUSIC_HEADER_BYTES) / sizeof(MusicEvent_t)) return 0U;

    for (i = 0U; i < 18U; i++)
    {
        sum = (uint8_t)(sum + hdr[i]);
    }

    addr = (uint32_t)sector * SFLASH_SECTOR_SIZE + MUSIC_HEADER_BYTES;
    remain = (uint32_t)count * sizeof(MusicEvent_t);

    while (remain > 0U)
    {
        uint32_t n = (remain > MUSIC_CHECK_CHUNK) ? MUSIC_CHECK_CHUNK : remain;

        if (SFlash_Read(addr, chunk, n) != SFLASH_OK) return 0U;
        for (i = 0U; i < n; i++)
        {
            sum = (uint8_t)(sum + chunk[i]);
        }
        addr += n;
        remain -= n;
    }

    return sum;
}
