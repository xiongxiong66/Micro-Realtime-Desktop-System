#ifndef __OLED_SYS_H
#define __OLED_SYS_H

#include "cmsis_os.h"
#include "Sw_Adc_Sys.h"
#include "oled.h"

extern osMessageQueueId_t cursorHandle;
extern osMessageQueueId_t KeyHandle;
extern osThreadId_t oledHandle;

void Oled_Task_Sys();

#endif
