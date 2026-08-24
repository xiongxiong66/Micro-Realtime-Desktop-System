/**
  ******************************************************************************
  * @file           : NameEdit_App.h
  * @brief          : Shared keypad name editor header
  ******************************************************************************
  */
#ifndef __NAMEEDIT_SYS_H
#define __NAMEEDIT_SYS_H

#include <stdint.h>

uint8_t NameEdit_Run(char *name, uint8_t max_len, uint8_t prefill,
                     uint8_t (*check_dup)(const char *new_name, const char *old_name));

#endif /* __NAMEEDIT_SYS_H */
