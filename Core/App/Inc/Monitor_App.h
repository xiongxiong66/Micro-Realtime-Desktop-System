/**
  ******************************************************************************
  * @file           : Monitor_App.h
  * @brief          : System monitor app header
  ******************************************************************************
  */
#ifndef __MONITOR_SYS_H
#define __MONITOR_SYS_H

#include <stdint.h>

void Monitor_Sys_Run(void);
void Monitor_Sys_ReportError(void);
uint32_t Monitor_Sys_GetErrorCount(void);

#endif /* __MONITOR_SYS_H */
