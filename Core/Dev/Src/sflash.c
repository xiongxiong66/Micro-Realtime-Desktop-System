/**
  ******************************************************************************
  * @file           : sflash.c
  * @brief          : Serial NOR flash device layer
  ******************************************************************************
  */

#include "sflash.h"
#include "w25q64.h"

static SFlash_Status_t sflash_map(BSP_W25Q64_Status_t st)
{
    return (st == BSP_W25Q64_OK) ? SFLASH_OK : SFLASH_ERROR;
}

SFlash_Status_t SFlash_Init(void)
{
    return sflash_map(BSP_W25Q64_Init());
}

SFlash_Status_t SFlash_EraseSector(uint32_t sector_idx)
{
    if (sector_idx >= SFLASH_SECTOR_COUNT) return SFLASH_ERR_INDEX;
    return sflash_map(BSP_W25Q64_EraseSector(sector_idx * SFLASH_SECTOR_SIZE));
}

SFlash_Status_t SFlash_SaveSector(uint32_t sector_idx, const uint8_t *p_data, uint32_t len)
{
    SFlash_Status_t st;

    if (sector_idx >= SFLASH_SECTOR_COUNT) return SFLASH_ERR_INDEX;
    if (p_data == NULL || len > SFLASH_SECTOR_SIZE) return SFLASH_ERR_PARAM;

    st = SFlash_EraseSector(sector_idx);
    if (st != SFLASH_OK) return st;

    return sflash_map(BSP_W25Q64_Write(sector_idx * SFLASH_SECTOR_SIZE, p_data, len));
}

SFlash_Status_t SFlash_LoadSector(uint32_t sector_idx, uint8_t *p_data, uint32_t len)
{
    if (sector_idx >= SFLASH_SECTOR_COUNT) return SFLASH_ERR_INDEX;
    if (p_data == NULL || len > SFLASH_SECTOR_SIZE) return SFLASH_ERR_PARAM;
    return sflash_map(BSP_W25Q64_Read(sector_idx * SFLASH_SECTOR_SIZE, p_data, len));
}

SFlash_Status_t SFlash_Read(uint32_t addr, uint8_t *p_data, uint32_t len)
{
    if (p_data == NULL || len == 0U) return SFLASH_ERR_PARAM;
    return sflash_map(BSP_W25Q64_Read(addr, p_data, len));
}

SFlash_Status_t SFlash_Write(uint32_t addr, const uint8_t *p_data, uint32_t len)
{
    if (p_data == NULL || len == 0U) return SFLASH_ERR_PARAM;
    return sflash_map(BSP_W25Q64_Write(addr, p_data, len));
}

SFlash_Status_t SFlash_EraseAll(void)
{
    return sflash_map(BSP_W25Q64_EraseChip());
}
