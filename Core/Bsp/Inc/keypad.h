/**
  ******************************************************************************
  * @file           : keypad.h
  * @brief          : 4x4 matrix keypad BSP driver
  ******************************************************************************
  */
#ifndef __BSP_KEYPAD_H
#define __BSP_KEYPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define BSP_KEYPAD_ROWS      4U
#define BSP_KEYPAD_COLS      4U
#define BSP_KEYPAD_NO_KEYCODE  0xFFU

typedef enum {
    BSP_KEYPAD_OK      = 0x00U,
    BSP_KEYPAD_NO_KEY  = 0x01U,
    BSP_KEYPAD_ERROR   = 0x02U
} BSP_Keypad_Status_t;

BSP_Keypad_Status_t BSP_Keypad_Init(void);
BSP_Keypad_Status_t BSP_Keypad_Scan(uint8_t *p_keycode);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_KEYPAD_H */
