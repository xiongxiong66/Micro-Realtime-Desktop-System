/**
  ******************************************************************************
  * @file           : Log_App.h
  * @brief          : System log task and viewer header
  ******************************************************************************
  */
#ifndef __LOG_SYS_H
#define __LOG_SYS_H

#include "cmsis_os.h"

#define LOG_TEXT_LEN  17U

#define LOG_TYPE_BOOT    1U
#define LOG_TYPE_SETTING 2U
#define LOG_TYPE_MUSIC   3U
#define LOG_TYPE_APP     4U
#define LOG_TYPE_ERROR   5U
#define LOG_TYPE_DRAW    6U

typedef struct {
    uint8_t type;
    char text[LOG_TEXT_LEN];
} LogMsg_t;

extern osMessageQueueId_t LogQueueHandle;

void Log_Write(uint8_t type, const char *text);
void Log_View_Run(void);
void Log_Task_Sys(void);

#endif /* __LOG_SYS_H */
