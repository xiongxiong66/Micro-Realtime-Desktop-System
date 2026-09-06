/**
  ******************************************************************************
  * @file           : ImgFile.c
  * @brief          : Shared image file checksum helpers
  ******************************************************************************
  */

#include "ImgFile.h"
#include "sflash.h"

uint8_t DrawFile_ComputeChecksum(const DrawFile_t *file)
{
    const uint8_t *p = (const uint8_t *)file;
    uint8_t sum = 0U;
    uint16_t i;

    if (file == NULL) return 0U;

    for (i = 0U; i < sizeof(DrawFile_t); i++)
    {
        if (i != 16U) /* checksum 字段本身 */
        {
            sum = (uint8_t)(sum + p[i]);
        }
    }
    return sum;
}

uint8_t DrawFile_MagicValid(uint16_t sector)
{
    uint8_t magic[4];

    if (SFlash_Read((uint32_t)sector * SFLASH_SECTOR_SIZE,
                    magic, sizeof(magic)) != SFLASH_OK) return 0U;

    return (magic[0] == DRAW_MAGIC0
         && magic[1] == DRAW_MAGIC1
         && magic[2] == DRAW_MAGIC2
         && magic[3] == DRAW_MAGIC3) ? 1U : 0U;
}

uint8_t DrawFile_LoadValid(uint16_t sector, DrawFile_t *out)
{
    if (out == NULL) return 0U;
    if (!DrawFile_MagicValid(sector)) return 0U;

    if (SFlash_LoadSector(sector, (uint8_t *)out, sizeof(DrawFile_t)) != SFLASH_OK) return 0U;

    return (out->checksum == DrawFile_ComputeChecksum(out)) ? 1U : 0U;
}
