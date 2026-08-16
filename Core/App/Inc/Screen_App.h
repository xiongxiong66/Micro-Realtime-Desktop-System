/**
  ******************************************************************************
  * @file           : Screen_Sys.h
  * @brief          : Screen timeout and wake-up control header
  ******************************************************************************
  */
#ifndef __SCREEN_SYS_H
#define __SCREEN_SYS_H

#include <stdint.h>

void Screen_Sys_Init(void);
void Screen_Sys_Wake(void);
void Screen_Sys_Update(void);
void Screen_Sys_ForceOff(void);
void Screen_Sys_SetForceOff(uint8_t enable);
uint8_t Screen_Sys_ForceOffEnabled(void);

#endif /* __SCREEN_SYS_H */
