/**
  ******************************************************************************
  * @file           : i2c.h
  * @brief          : I2C BSP 驱动头文件
  ******************************************************************************
  */
#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BSP_I2C_TIMEOUT      1000U
#define BSP_I2C_RETRY_TIMES  3U

typedef enum {
    BSP_I2C_OK       = 0x00U,
    BSP_I2C_ERROR    = 0x01U,
    BSP_I2C_BUSY     = 0x02U,
    BSP_I2C_ERR_TIMEOUT = 0x03U,
    BSP_I2C_NACK     = 0x04U
} BSP_I2C_Status_t;

BSP_I2C_Status_t BSP_I2C1_Init(void);
BSP_I2C_Status_t BSP_I2C1_DeInit(void);
BSP_I2C_Status_t BSP_I2C_Master_Transmit(uint16_t dev_addr, uint8_t *p_data, uint16_t size);
BSP_I2C_Status_t BSP_I2C_Master_Receive(uint16_t dev_addr, uint8_t *p_data, uint16_t size);
BSP_I2C_Status_t BSP_I2C_Mem_Write(uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_addr_size, uint8_t *p_data, uint16_t size);
BSP_I2C_Status_t BSP_I2C_Mem_Read(uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_addr_size, uint8_t *p_data, uint16_t size);
BSP_I2C_Status_t BSP_I2C_IsDeviceReady(uint16_t dev_addr);
int BSP_I2C_Scan(uint8_t *p_devices, int max_count);
BSP_I2C_Status_t BSP_I2C_Translate_HAL_Status(HAL_StatusTypeDef hal_status);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_I2C_H */
