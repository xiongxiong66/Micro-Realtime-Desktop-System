/**
  ******************************************************************************
  * @file           : adc.h
  * @brief          : ADC BSP 驱动头文件
  ******************************************************************************
  */
#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BSP_ADC_REF_VOLTAGE_MV  3300U
#define BSP_ADC_RESOLUTION      4095U
#define BSP_ADC_TIMEOUT         100U

typedef enum {
    BSP_ADC_OK      = 0x00U,
    BSP_ADC_ERROR   = 0x01U,
    BSP_ADC_BUSY    = 0x02U,
    BSP_ADC_ERR_TIMEOUT = 0x03U
} BSP_ADC_Status_t;

BSP_ADC_Status_t BSP_ADC1_Init(void);
BSP_ADC_Status_t BSP_ADC1_DeInit(void);
BSP_ADC_Status_t BSP_ADC_Start(void);
BSP_ADC_Status_t BSP_ADC_Stop(void);
BSP_ADC_Status_t BSP_ADC_ReadDual(uint16_t *p_ch0_val, uint16_t *p_ch1_val);
uint16_t BSP_ADC_ConvertToMillivolt(uint16_t adc_value);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ADC_H */
