/**
  ******************************************************************************
  * @file           : File_Sys.h
  * @brief          : File manager app task header
  ******************************************************************************
  */
#ifndef __FILE_SYS_H
#define __FILE_SYS_H

#include "cmsis_os.h"

extern osThreadId_t App_FileHandle;

void App_File_Task_Sys(void);

#endif /* __FILE_SYS_H */
