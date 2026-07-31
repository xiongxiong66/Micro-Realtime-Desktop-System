/**
  ******************************************************************************
  * @file           : MKey_Sys.h
  * @brief          : Matrix keypad task header
  ******************************************************************************
  */
#ifndef __MKEY_SYS_H
#define __MKEY_SYS_H

#include "cmsis_os.h"

extern osMessageQueueId_t KeyHandle;

void MKey_Task_Sys(void);

#endif /* __MKEY_SYS_H */
