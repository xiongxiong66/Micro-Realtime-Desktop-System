/**
  ******************************************************************************
  * @file           : w25q64.h
  * @brief          : W25Q64 SPI NOR Flash BSP driver
  ******************************************************************************
  */
#ifndef __BSP_W25Q64_H
#define __BSP_W25Q64_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BSP_W25Q64_PAGE_SIZE      256U
#define BSP_W25Q64_SECTOR_SIZE    4096U
#define BSP_W25Q64_CAPACITY       0x800000UL
#define BSP_W25Q64_MFG_WINBOND    0xEFU

typedef enum {
    BSP_W25Q64_OK          = 0x00U,
    BSP_W25Q64_ERROR       = 0x01U,
    BSP_W25Q64_BUSY        = 0x02U,
    BSP_W25Q64_ERR_TIMEOUT = 0x03U,
    BSP_W25Q64_ERR_ADDR    = 0x04U,
    BSP_W25Q64_ERR_ID      = 0x05U
} BSP_W25Q64_Status_t;

typedef struct {
    uint8_t mfg;       /* JEDEC manufacturer ID (Winbond = 0xEF) */
    uint8_t mem_type;  /* memory type (W25Q64 = 0x40) */
    uint8_t capacity;  /* capacity ID (W25Q64 = 0x17) */
} BSP_W25Q64_ID_t;

BSP_W25Q64_Status_t BSP_W25Q64_Init(void);
BSP_W25Q64_Status_t BSP_W25Q64_ReadID(BSP_W25Q64_ID_t *p_id);
BSP_W25Q64_Status_t BSP_W25Q64_Read(uint32_t addr, uint8_t *p_data, uint32_t len);
/* Page programming can only change 1 -> 0; the target region must be erased
   (BSP_W25Q64_EraseSector / BSP_W25Q64_EraseChip) before BSP_W25Q64_Write. */
BSP_W25Q64_Status_t BSP_W25Q64_Write(uint32_t addr, const uint8_t *p_data, uint32_t len);
BSP_W25Q64_Status_t BSP_W25Q64_EraseSector(uint32_t addr);
BSP_W25Q64_Status_t BSP_W25Q64_EraseChip(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_W25Q64_H */
