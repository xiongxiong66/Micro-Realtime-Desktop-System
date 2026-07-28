#include "i2c.h"
extern I2C_HandleTypeDef hi2c1;

BSP_I2C_Status_t BSP_I2C_Translate_HAL_Status(HAL_StatusTypeDef s) {
    switch(s) {
        case HAL_OK: return BSP_I2C_OK;
        case HAL_BUSY: return BSP_I2C_BUSY;
        case HAL_TIMEOUT: return BSP_I2C_ERR_TIMEOUT;
        default: return BSP_I2C_ERROR;
    }
}
BSP_I2C_Status_t BSP_I2C1_Init(void) { return BSP_I2C_OK; }
BSP_I2C_Status_t BSP_I2C1_DeInit(void) { return BSP_I2C_Translate_HAL_Status(HAL_I2C_DeInit(&hi2c1)); }

static BSP_I2C_Status_t _mem_op(uint16_t dev, uint16_t mem, uint16_t mems, uint8_t *d, uint16_t sz, uint8_t is_read) {
    uint32_t r = BSP_I2C_RETRY_TIMES;
    HAL_StatusTypeDef hs;
    do {
        if (is_read) hs = HAL_I2C_Mem_Read(&hi2c1, dev<<1, mem, mems, d, sz, BSP_I2C_TIMEOUT);
        else hs = HAL_I2C_Mem_Write(&hi2c1, dev<<1, mem, mems, d, sz, BSP_I2C_TIMEOUT);
        if (hs == HAL_OK) return BSP_I2C_OK;
        r--;
    } while(r);
    return BSP_I2C_Translate_HAL_Status(hs);
}

BSP_I2C_Status_t BSP_I2C_Master_Transmit(uint16_t d, uint8_t *p, uint16_t s) { return _mem_op(d, 0, I2C_MEMADD_SIZE_8BIT, p, s, 0); }
BSP_I2C_Status_t BSP_I2C_Master_Receive(uint16_t d, uint8_t *p, uint16_t s) { return _mem_op(d, 0, I2C_MEMADD_SIZE_8BIT, p, s, 1); }
BSP_I2C_Status_t BSP_I2C_Mem_Write(uint16_t d, uint16_t m, uint16_t ms, uint8_t *p, uint16_t s) { return _mem_op(d, m, ms, p, s, 0); }
BSP_I2C_Status_t BSP_I2C_Mem_Read(uint16_t d, uint16_t m, uint16_t ms, uint8_t *p, uint16_t s) { return _mem_op(d, m, ms, p, s, 1); }

BSP_I2C_Status_t BSP_I2C_IsDeviceReady(uint16_t d) {
    return BSP_I2C_Translate_HAL_Status(HAL_I2C_IsDeviceReady(&hi2c1, d<<1, BSP_I2C_RETRY_TIMES, BSP_I2C_TIMEOUT));
}
int BSP_I2C_Scan(uint8_t *pd, int mc) {
    int f = 0;
    for (uint16_t a = 1; a < 0x7F && f < mc; a++)
        if (HAL_I2C_IsDeviceReady(&hi2c1, a<<1, BSP_I2C_RETRY_TIMES, BSP_I2C_TIMEOUT) == HAL_OK) pd[f++] = a;
    return f;
}
