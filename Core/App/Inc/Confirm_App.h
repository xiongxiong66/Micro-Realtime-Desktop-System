/**
  ******************************************************************************
  * @file           : Confirm_App.h
  * @brief          : Shared confirm dialog header
  ******************************************************************************
  */
#ifndef __CONFIRM_SYS_H
#define __CONFIRM_SYS_H

#include <stdint.h>

uint8_t Confirm_Ask(const char *title);
uint8_t Confirm_Delete(const char *name);

#endif /* __CONFIRM_SYS_H */
