/**
  ******************************************************************************
  * @file           : Set_App.h
  * @brief          : Settings app and persistent configuration header
  ******************************************************************************
  */
#ifndef __SET_SYS_H
#define __SET_SYS_H

#include <stdint.h>

#define SETTINGS_SECTOR_BASE  0U
#define SETTINGS_LEVELS       3U
#define SETTINGS_PIN_MAX_LEN  10U       //密码最大长度

typedef struct {
    uint8_t cursor_size;   /* 0 small, 1 medium, 2 large */
    uint8_t sensitivity;   /* 0 low, 1 medium, 2 high */
    uint8_t brightness;    /* 0 low, 1 medium, 2 high */
    uint8_t volume;        /* 0 mute, 1..5 louder */
    uint8_t screen_timeout;/* 0..4 -> 10/15/20/25/30 s */
    char password[SETTINGS_PIN_MAX_LEN + 1U];
} SetConfig_t;

void Set_Sys_Load(void);
void Set_Sys_Save(void);
void Set_Sys_Run(void);
void Set_Sys_ApplyBrightness(void);
void Set_Sys_ApplyVolume(void);

uint8_t Set_Sys_GetCursorSize(void);
uint8_t Set_Sys_GetCursorPixels(void);
uint8_t Set_Sys_GetSensitivity(void);
uint8_t Set_Sys_GetBrightness(void);
uint8_t Set_Sys_GetVolume(void);
uint32_t Set_Sys_GetScreenTimeoutMs(void);
uint8_t Set_Sys_CheckPin(const char *pin);
void Set_Sys_ChangePin(const char *pin);
uint8_t Set_Sys_LoadValid(void);

#endif /* __SET_SYS_H */
