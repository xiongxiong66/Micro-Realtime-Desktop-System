/**
  ******************************************************************************
  * @file           : DS3231.c
  * @brief          : DS3231 RTC driver over I2C2
  ******************************************************************************
  */

#include "DS3231.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c2;

#define DS3231_REG_SECONDS   0x00U
#define DS3231_REG_MINUTES   0x01U
#define DS3231_REG_HOURS     0x02U
#define DS3231_REG_DAY       0x03U
#define DS3231_REG_DATE      0x04U
#define DS3231_REG_MONTH     0x05U
#define DS3231_REG_YEAR      0x06U
#define DS3231_I2C_TIMEOUT   100U

static uint8_t DS3231_BCD2Bin(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) & 0x0FU) * 10U + (bcd & 0x0FU));
}

static uint8_t DS3231_Bin2BCD(uint8_t bin)
{
    return (uint8_t)(((bin / 10U) << 4) | (bin % 10U));
}

uint8_t DS3231_Init(void)
{
    return HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(DS3231_ADDR << 1U),
                                 3U, DS3231_I2C_TIMEOUT) == HAL_OK ? 1U : 0U;
}

uint8_t DS3231_ReadTime(DS3231_Time_t *time)
{
    uint8_t regs[7];

    if (time == NULL) return 0U;
    if (HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(DS3231_ADDR << 1U),
                          DS3231_REG_SECONDS, I2C_MEMADD_SIZE_8BIT,
                          regs, sizeof(regs), DS3231_I2C_TIMEOUT) != HAL_OK)
    {
        return 0U;
    }

    time->second = DS3231_BCD2Bin((uint8_t)(regs[0] & 0x7FU));
    time->minute = DS3231_BCD2Bin((uint8_t)(regs[1] & 0x7FU));
    time->hour = DS3231_BCD2Bin((uint8_t)(regs[2] & 0x3FU));
    time->day = DS3231_BCD2Bin((uint8_t)(regs[4] & 0x3FU));
    time->month = DS3231_BCD2Bin((uint8_t)(regs[5] & 0x1FU));
    time->year = DS3231_BCD2Bin(regs[6]);
    return 1U;
}

uint8_t DS3231_WriteTime(const DS3231_Time_t *time)
{
    uint8_t regs[7];

    if (time == NULL) return 0U;

    regs[0] = DS3231_Bin2BCD(time->second);
    regs[1] = DS3231_Bin2BCD(time->minute);
    regs[2] = DS3231_Bin2BCD(time->hour);
    regs[3] = 1U;
    regs[4] = DS3231_Bin2BCD(time->day);
    regs[5] = DS3231_Bin2BCD(time->month);
    regs[6] = DS3231_Bin2BCD(time->year);

    return HAL_I2C_Mem_Write(&hi2c2, (uint16_t)(DS3231_ADDR << 1U),
                             DS3231_REG_SECONDS, I2C_MEMADD_SIZE_8BIT,
                             regs, sizeof(regs), DS3231_I2C_TIMEOUT) == HAL_OK ? 1U : 0U;
}
