/**
  ******************************************************************************
  * @file           : DS3231.h
  * @brief          : DS3231 RTC driver header
  ******************************************************************************
  */
#ifndef __DS3231_H
#define __DS3231_H

#include <stdint.h>

#define DS3231_ADDR  0x68U

typedef struct {
    uint8_t year;    /* 00-99 */
    uint8_t month;   /* 1-12 */
    uint8_t day;     /* 1-31 */
    uint8_t hour;    /* 0-23 */
    uint8_t minute;  /* 0-59 */
    uint8_t second;  /* 0-59 */
} DS3231_Time_t;

uint8_t DS3231_Init(void);
uint8_t DS3231_ReadTime(DS3231_Time_t *time);
uint8_t DS3231_WriteTime(const DS3231_Time_t *time);

#endif /* __DS3231_H */
