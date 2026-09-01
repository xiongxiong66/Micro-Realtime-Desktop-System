/**
  ******************************************************************************
  * @file           : sflash.c
  * @brief          : Serial NOR flash device layer
  ******************************************************************************
  */

#include "sflash.h"
#include "w25q64.h"
#include "cmsis_os.h"

static osMutexId_t sflash_mutex = NULL;

static void sflash_acquire(void)
{
    if (osKernelGetState() == osKernelRunning)
    {
        if (sflash_mutex == NULL)
        {
            sflash_mutex = osMutexNew(NULL);
        }
        if (sflash_mutex != NULL)
        {
            (void)osMutexAcquire(sflash_mutex, 500U);
        }
    }
}

static void sflash_release(void)
{
    if (sflash_mutex != NULL)
    {
        (void)osMutexRelease(sflash_mutex);
    }
}

static SFlash_Status_t sflash_map(BSP_W25Q64_Status_t st)
{
    return (st == BSP_W25Q64_OK) ? SFLASH_OK : SFLASH_ERROR;
}

SFlash_Status_t SFlash_Init(void)
{
    SFlash_Status_t st;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_Init());
    sflash_release();
    return st;
}

SFlash_Status_t SFlash_EraseSector(uint32_t sector_idx)
{
    SFlash_Status_t st;

    if (sector_idx >= SFLASH_SECTOR_COUNT) return SFLASH_ERR_INDEX;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_EraseSector(sector_idx * SFLASH_SECTOR_SIZE));
    sflash_release();
    return st;
}

SFlash_Status_t SFlash_SaveSector(uint32_t sector_idx, const uint8_t *p_data, uint32_t len)
{
    SFlash_Status_t st;

    if (sector_idx >= SFLASH_SECTOR_COUNT) return SFLASH_ERR_INDEX;
    if (p_data == NULL || len > SFLASH_SECTOR_SIZE) return SFLASH_ERR_PARAM;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_EraseSector(sector_idx * SFLASH_SECTOR_SIZE));
    if (st == SFLASH_OK)
    {
        st = sflash_map(BSP_W25Q64_Write(sector_idx * SFLASH_SECTOR_SIZE, p_data, len));
    }
    sflash_release();

    return st;
}

SFlash_Status_t SFlash_LoadSector(uint32_t sector_idx, uint8_t *p_data, uint32_t len)
{
    if (sector_idx >= SFLASH_SECTOR_COUNT) return SFLASH_ERR_INDEX;
    if (p_data == NULL || len > SFLASH_SECTOR_SIZE) return SFLASH_ERR_PARAM;

    SFlash_Status_t st;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_Read(sector_idx * SFLASH_SECTOR_SIZE, p_data, len));
    sflash_release();
    return st;
}

SFlash_Status_t SFlash_Read(uint32_t addr, uint8_t *p_data, uint32_t len)
{
    if (p_data == NULL || len == 0U) return SFLASH_ERR_PARAM;

    SFlash_Status_t st;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_Read(addr, p_data, len));
    sflash_release();
    return st;
}

SFlash_Status_t SFlash_Write(uint32_t addr, const uint8_t *p_data, uint32_t len)
{
    if (p_data == NULL || len == 0U) return SFLASH_ERR_PARAM;

    SFlash_Status_t st;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_Write(addr, p_data, len));
    sflash_release();
    return st;
}

SFlash_Status_t SFlash_EraseAll(void)
{
    SFlash_Status_t st;

    sflash_acquire();
    st = sflash_map(BSP_W25Q64_EraseChip());
    sflash_release();
    return st;
}
