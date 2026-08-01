/**
  ******************************************************************************
  * @file           : sflash.h
  * @brief          : Serial NOR flash device layer (sector based)
  ******************************************************************************
  */
#ifndef __SFLASH_H
#define __SFLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SFLASH_SECTOR_SIZE   4096U
#define SFLASH_SECTOR_COUNT  2048U

typedef enum {
    SFLASH_OK        = 0x00U,
    SFLASH_ERROR     = 0x01U,
    SFLASH_ERR_INDEX = 0x02U,
    SFLASH_ERR_PARAM = 0x03U
} SFlash_Status_t;

SFlash_Status_t SFlash_Init(void);
SFlash_Status_t SFlash_EraseSector(uint32_t sector_idx);
SFlash_Status_t SFlash_SaveSector(uint32_t sector_idx, const uint8_t *p_data, uint32_t len);
SFlash_Status_t SFlash_LoadSector(uint32_t sector_idx, uint8_t *p_data, uint32_t len);
SFlash_Status_t SFlash_Read(uint32_t addr, uint8_t *p_data, uint32_t len);
SFlash_Status_t SFlash_Write(uint32_t addr, const uint8_t *p_data, uint32_t len);
SFlash_Status_t SFlash_EraseAll(void);

#ifdef __cplusplus
}
#endif

#endif /* __SFLASH_H */
