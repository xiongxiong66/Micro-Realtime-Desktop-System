/**
  ******************************************************************************
  * @file           : Music_Sys.h
  * @brief          : Music app task header
  ******************************************************************************
  */
#ifndef __MUSIC_SYS_H
#define __MUSIC_SYS_H

#include "cmsis_os.h"
#include <stdint.h>

extern osThreadId_t App_MusicHandle;
extern osThreadId_t MusicPlayHandle;

void App_Music_Task_Sys(void);
void Music_Play_Task_Sys(void);
void Music_Play(uint8_t idx);
void Music_Rename(uint8_t idx);
void Music_Delete(uint8_t idx);
void Music_Bg_Start(uint8_t idx);
void Music_Bg_Toggle(void);
uint8_t Music_Bg_IsPlaying(void);

#endif /* __MUSIC_SYS_H */
