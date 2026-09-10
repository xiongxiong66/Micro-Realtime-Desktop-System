/**
  ******************************************************************************
  * @file           : UiInput.c
  * @brief          : UI key wait wrapper for task heartbeat monitoring
  ******************************************************************************
  */

#include "UiInput.h"
#include "TaskWatch.h"

extern osMessageQueueId_t KeyHandle;

osStatus_t Ui_KeyGet(char *key, uint32_t timeout)
{
    uint8_t task_id = TaskWatch_CurrentUiId();
    osStatus_t status;

    TaskWatch_WaitBegin(task_id);
    status = osMessageQueueGet(KeyHandle, key, NULL, timeout);
    TaskWatch_WaitEnd(task_id);

    return status;
}
