#include "adc.h"
extern ADC_HandleTypeDef hadc1;

static uint32_t adc_dma_buf[2];
static volatile uint8_t adc_dma_done = 0U;
static volatile uint8_t adc_dma_error = 0U;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc_dma_done = 1U;
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc_dma_error = 1U;
    }
}

BSP_ADC_Status_t BSP_ADC1_Init(void) { return BSP_ADC_OK; }
BSP_ADC_Status_t BSP_ADC1_DeInit(void) {
    return (HAL_ADC_DeInit(&hadc1) == HAL_OK) ? BSP_ADC_OK : BSP_ADC_ERROR;
}
BSP_ADC_Status_t BSP_ADC_Start(void) {
    return (HAL_ADC_Start(&hadc1) == HAL_OK) ? BSP_ADC_OK : BSP_ADC_ERROR;
}
BSP_ADC_Status_t BSP_ADC_Stop(void) {
    return (HAL_ADC_Stop(&hadc1) == HAL_OK) ? BSP_ADC_OK : BSP_ADC_ERROR;
}
BSP_ADC_Status_t BSP_ADC_ReadDual(uint16_t *c0, uint16_t *c1) {
    uint32_t tickstart;

    if (!c0 || !c1) return BSP_ADC_ERROR;

    adc_dma_done = 0U;
    adc_dma_error = 0U;

    if (HAL_ADC_Start_DMA(&hadc1, adc_dma_buf, 2U) != HAL_OK)
    {
        return BSP_ADC_ERROR;
    }

    tickstart = HAL_GetTick();
    while (adc_dma_done == 0U && adc_dma_error == 0U)
    {
        if ((HAL_GetTick() - tickstart) >= BSP_ADC_TIMEOUT)
        {
            HAL_ADC_Stop_DMA(&hadc1);
            return BSP_ADC_ERR_TIMEOUT;
        }
    }

    HAL_ADC_Stop_DMA(&hadc1);

    if (adc_dma_error != 0U)
    {
        return BSP_ADC_ERROR;
    }

    /* DMA writes rank1 (CH0) to buf[0] and rank2 (CH1) to buf[1]. */
    *c0 = (uint16_t)adc_dma_buf[0];
    *c1 = (uint16_t)adc_dma_buf[1];
    return BSP_ADC_OK;
}
uint16_t BSP_ADC_ConvertToMillivolt(uint16_t v) {
    return (uint16_t)((uint32_t)v * BSP_ADC_REF_VOLTAGE_MV / BSP_ADC_RESOLUTION);
}
