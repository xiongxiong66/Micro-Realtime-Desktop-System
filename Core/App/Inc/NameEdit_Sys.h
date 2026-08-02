/**
  ******************************************************************************
  * @file           : NameEdit_Sys.h
  * @brief          : Shared keypad name editor header
  ******************************************************************************
  */
#ifndef __NAMEEDIT_SYS_H
#define __NAMEEDIT_SYS_H

#include <stdint.h>

uint8_t NameEdit_Run(char *name, uint8_t max_len, uint8_t prefill);

#endif /* __NAMEEDIT_SYS_H */
