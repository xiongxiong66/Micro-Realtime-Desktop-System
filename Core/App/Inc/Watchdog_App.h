/**
  ******************************************************************************
  * @file           : Watchdog_App.h
  * @brief          : IWDG watchdog task: heartbeat check + feed
  ******************************************************************************
  */
#ifndef __WATCHDOG_APP_H
#define __WATCHDOG_APP_H

void Watchdog_Init(void);
void Watchdog_Task_Sys(void);

#endif /* __WATCHDOG_APP_H */
