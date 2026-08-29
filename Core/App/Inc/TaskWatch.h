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
#define TASKWATCH_COUNT 3U

void TaskWatch_Beat(uint8_t id);
void TaskWatch_Check(void);
uint8_t TaskWatch_IsOk(uint8_t id);

#endif /* __TASK_WATCH_H */
