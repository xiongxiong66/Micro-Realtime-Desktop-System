#ifndef __Sw_Adc_Sys_H
#define __Sw_Adc_Sys_H

#include "stdint.h"
#include "adc.h"

#define Statue_NO 0
#define Statue_Yes 1

extern uint8_t state;

extern volatile uint32_t adc_events;
extern volatile uint32_t adc_dropped;

typedef struct {
    int16_t cursor_x;
    int16_t cursor_y;
    uint8_t  button_pressed;
} CursorMsg_t;

void Sw_Adc_Task_Sys();

#endif