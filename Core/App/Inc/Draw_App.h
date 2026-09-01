/**
  ******************************************************************************
  * @file           : Draw_App.h
  * @brief          : Drawing app task header
  ******************************************************************************
  */
#ifndef __DRAW_SYS_H
#define __DRAW_SYS_H

#include <stdint.h>

void Draw_Sys_Run(void);
void Draw_App_New(void);
uint8_t Draw_NameUsed(const char *new_name, const char *old_name);

#endif /* __DRAW_SYS_H */
