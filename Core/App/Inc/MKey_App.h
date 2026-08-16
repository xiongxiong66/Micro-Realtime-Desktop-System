/**
  ******************************************************************************
  * @file           : MKey_App.h
  * @brief          : Matrix keypad task header
  ******************************************************************************
  */
#ifndef __MKEY_SYS_H
#define __MKEY_SYS_H

#include "cmsis_os.h"

extern osMessageQueueId_t KeyHandle;

extern volatile uint32_t mkey_events;
extern volatile uint32_t mkey_dropped;

void MKey_Task_Sys(void);

#endif /* __MKEY_SYS_H */
