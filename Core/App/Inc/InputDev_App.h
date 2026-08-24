/**
  ******************************************************************************
  * @file           : InputDev_App.h
  * @brief          : Input device connection detection (joystick)
  ******************************************************************************
  */
#ifndef __INPUTDEV_APP_H
#define __INPUTDEV_APP_H

#include <stdint.h>

void InputDev_MonitorSample(uint16_t x, uint16_t y, uint16_t center_x, uint16_t center_y);
uint8_t InputDev_IsConnected(void);

#endif /* __INPUTDEV_APP_H */
