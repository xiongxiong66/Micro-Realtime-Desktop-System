/**
  ******************************************************************************
  * @file           : UiInput.h
  * @brief          : UI key wait wrapper for task heartbeat monitoring
  ******************************************************************************
  */
#ifndef __UI_INPUT_H
#define __UI_INPUT_H

#include "cmsis_os.h"

osStatus_t Ui_KeyGet(char *key, uint32_t timeout);

#endif /* __UI_INPUT_H */
