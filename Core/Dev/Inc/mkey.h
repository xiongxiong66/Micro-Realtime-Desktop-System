/**
  ******************************************************************************
  * @file           : mkey.h
  * @brief          : 4x4 matrix keypad device layer
  ******************************************************************************
  */
#ifndef __MKEY_H
#define __MKEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MKEY_DEBOUNCE_MS  20U

typedef enum {
    MKEY_OK      = 0x00U,
    MKEY_NO_KEY  = 0x01U
} MKey_Status_t;

char MKey_KeyToChar(uint8_t keycode);
MKey_Status_t MKey_GetKey(char *p_key);
MKey_Status_t MKey_GetKeyEvent(char *p_key);

#ifdef __cplusplus
}
#endif

#endif /* __MKEY_H */
