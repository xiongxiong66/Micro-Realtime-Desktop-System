/**
  ******************************************************************************
  * @file           : Draw_Sys.h
  * @brief          : Drawing app task header
  ******************************************************************************
  */
#ifndef __DRAW_SYS_H
#define __DRAW_SYS_H

#include "cmsis_os.h"

extern osThreadId_t App_DrawHandle;

void App_Draw_Task_Sys(void);

#endif /* __DRAW_SYS_H */
