/**
  ******************************************************************************
  * @file           : Screen_App.h
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
uint8_t Screen_Sys_IsOff(void);
uint8_t Screen_Sys_IsDeepOff(void);
uint8_t Screen_Sys_IsLocking(void);
void Screen_Lock_Task_Sys(void);

#endif /* __SCREEN_SYS_H */
