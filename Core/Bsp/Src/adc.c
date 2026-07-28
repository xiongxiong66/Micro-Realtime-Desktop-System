#include "adc.h"
extern ADC_HandleTypeDef hadc1;

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
    if (!c0 || !c1) return BSP_ADC_ERROR;
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return BSP_ADC_ERROR;
    if (HAL_ADC_PollForConversion(&hadc1, BSP_ADC_TIMEOUT) != HAL_OK) { HAL_ADC_Stop(&hadc1); return BSP_ADC_ERR_TIMEOUT; }
    (void)HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, BSP_ADC_TIMEOUT) != HAL_OK) { HAL_ADC_Stop(&hadc1); return BSP_ADC_ERR_TIMEOUT; }
    *c0 = HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, BSP_ADC_TIMEOUT) != HAL_OK) { HAL_ADC_Stop(&hadc1); return BSP_ADC_ERR_TIMEOUT; }
    *c1 = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return BSP_ADC_OK;
}
uint16_t BSP_ADC_ConvertToMillivolt(uint16_t v) {
    return (uint16_t)((uint32_t)v * BSP_ADC_REF_VOLTAGE_MV / BSP_ADC_RESOLUTION);
}
