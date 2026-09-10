/**
  ******************************************************************************
  * @file           : TaskWatch.h
  * @brief          : Task heartbeat and hang detection
  ******************************************************************************
  */
#ifndef __TASK_WATCH_H
#define __TASK_WATCH_H

#include <stdint.h>

#define TASKWATCH_MKEY  0U
#define TASKWATCH_ADC   1U
#define TASKWATCH_LOG   2U
#define TASKWATCH_OLED  3U
#define TASKWATCH_FILE  4U
#define TASKWATCH_MUSIC 5U
#define TASKWATCH_COUNT 6U

void TaskWatch_Beat(uint8_t id);
void TaskWatch_Refresh(uint8_t id);
void TaskWatch_WaitBegin(uint8_t id);
void TaskWatch_WaitEnd(uint8_t id);
uint8_t TaskWatch_CurrentUiId(void);
void TaskWatch_BeatCurrentUi(void);
void TaskWatch_DebugRequestCurrentUi(void);
void TaskWatch_Check(void);
uint8_t TaskWatch_IsOk(uint8_t id);
uint8_t TaskWatch_AnyHang(void);

#endif /* __TASK_WATCH_H */
