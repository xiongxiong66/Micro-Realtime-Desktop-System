#include "i2c.h"
#include "cmsis_os.h"
extern I2C_HandleTypeDef hi2c1;

static osMutexId_t i2c1_mutex = NULL;

/* I2C1 被 OLED 任务和 Screen（息屏/唤醒）共用，用互斥锁串行化访问，
   避免 MKey 在 OLED 传输中途抢占后挂起任务，把 I2C 总线卡住。 */
static void i2c1_acquire(void)
{
    if (osKernelGetState() == osKernelRunning)
    {
        if (i2c1_mutex == NULL)
        {
            i2c1_mutex = osMutexNew(NULL);
        }
        if (i2c1_mutex != NULL)
        {
            (void)osMutexAcquire(i2c1_mutex, 500U);
        }
    }
}

static void i2c1_release(void)
{
    if (i2c1_mutex != NULL)
    {
        (void)osMutexRelease(i2c1_mutex);
    }
}

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
    BSP_I2C_Status_t st;

    i2c1_acquire();
    do {
        if (is_read) hs = HAL_I2C_Mem_Read(&hi2c1, dev<<1, mem, mems, d, sz, BSP_I2C_TIMEOUT);
        else hs = HAL_I2C_Mem_Write(&hi2c1, dev<<1, mem, mems, d, sz, BSP_I2C_TIMEOUT);
        if (hs == HAL_OK)
        {
            i2c1_release();
            return BSP_I2C_OK;
        }
        r--;
    } while(r);
    st = BSP_I2C_Translate_HAL_Status(hs);
    i2c1_release();
    return st;
}

BSP_I2C_Status_t BSP_I2C_Master_Transmit(uint16_t d, uint8_t *p, uint16_t s) { return _mem_op(d, 0, I2C_MEMADD_SIZE_8BIT, p, s, 0); }
BSP_I2C_Status_t BSP_I2C_Master_Receive(uint16_t d, uint8_t *p, uint16_t s) { return _mem_op(d, 0, I2C_MEMADD_SIZE_8BIT, p, s, 1); }
BSP_I2C_Status_t BSP_I2C_Mem_Write(uint16_t d, uint16_t m, uint16_t ms, uint8_t *p, uint16_t s) { return _mem_op(d, m, ms, p, s, 0); }
BSP_I2C_Status_t BSP_I2C_Mem_Read(uint16_t d, uint16_t m, uint16_t ms, uint8_t *p, uint16_t s) { return _mem_op(d, m, ms, p, s, 1); }

BSP_I2C_Status_t BSP_I2C_IsDeviceReady(uint16_t d) {
    BSP_I2C_Status_t st;

    i2c1_acquire();
    st = BSP_I2C_Translate_HAL_Status(HAL_I2C_IsDeviceReady(&hi2c1, d<<1, BSP_I2C_RETRY_TIMES, BSP_I2C_TIMEOUT));
    i2c1_release();
    return st;
}
int BSP_I2C_Scan(uint8_t *pd, int mc) {
    int f = 0;

    i2c1_acquire();
    for (uint16_t a = 1; a < 0x7F && f < mc; a++)
        if (HAL_I2C_IsDeviceReady(&hi2c1, a<<1, BSP_I2C_RETRY_TIMES, BSP_I2C_TIMEOUT) == HAL_OK) pd[f++] = a;
    i2c1_release();
    return f;
}
