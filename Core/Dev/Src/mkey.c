/**
  ******************************************************************************
  * @file           : mkey.c
  * @brief          : Debounced keypad reads and key-code to character mapping
  ******************************************************************************
  */

#include "mkey.h"
#include "keypad.h"
#include "main.h"

/* Layout: row0=1 2 3 A, row1=4 5 6 B, row2=7 8 9 C, row3=* 0 # D */
static const char mkey_char_map[BSP_KEYPAD_ROWS * BSP_KEYPAD_COLS] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'
};

static char mkey_raw_key = '\0';
static char mkey_stable_key = '\0';
static uint32_t mkey_change_tick = 0U;

char MKey_KeyToChar(uint8_t keycode)
{
    if (keycode >= (BSP_KEYPAD_ROWS * BSP_KEYPAD_COLS))
    {
        return '\0';
    }
    return mkey_char_map[keycode];
}

MKey_Status_t MKey_GetKey(char *p_key)
{
    uint8_t keycode;
    char raw;

    if (p_key == NULL)
    {
        return MKEY_NO_KEY;
    }

    raw = (BSP_Keypad_Scan(&keycode) == BSP_KEYPAD_OK) ? MKey_KeyToChar(keycode) : '\0';

    if (raw != mkey_raw_key)
    {
        mkey_raw_key = raw;
        mkey_change_tick = HAL_GetTick();
        mkey_stable_key = '\0';
    }
    else if (mkey_stable_key == '\0' && (HAL_GetTick() - mkey_change_tick) >= MKEY_DEBOUNCE_MS)
    {
        mkey_stable_key = raw;
    }

    *p_key = mkey_stable_key;
    return (*p_key == '\0') ? MKEY_NO_KEY : MKEY_OK;
}

MKey_Status_t MKey_GetKeyEvent(char *p_key)
{
    char cur;
    static char last_event_key = '\0';

    if (p_key == NULL)
    {
        return MKEY_NO_KEY;
    }

    if (MKey_GetKey(&cur) == MKEY_OK && cur != last_event_key)
    {
        last_event_key = cur;
        *p_key = cur;
        return MKEY_OK;
    }

    if (cur == '\0')
    {
        last_event_key = '\0';
    }
    return MKEY_NO_KEY;
}
