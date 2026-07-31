/**
  ******************************************************************************
  * @file           : keypad.c
  * @brief          : 4x4 matrix keypad scan (rows PB12-PB15, cols PA8-PA11)
  ******************************************************************************
  */

#include "keypad.h"

/* Row order: PB12(row0) PB13(row1) PB14(row2) PB15(row3).
   CubeMX labels: x4=PB12, x3=PB13, x2=PB14, x1=PB15. */
static GPIO_TypeDef *const keypad_row_port[BSP_KEYPAD_ROWS] = {
    MKey_x4_GPIO_Port, MKey_x3_GPIO_Port, MKey_x2_GPIO_Port, MKey_x1_GPIO_Port
};
static const uint16_t keypad_row_pin[BSP_KEYPAD_ROWS] = {
    MKey_x4_Pin, MKey_x3_Pin, MKey_x2_Pin, MKey_x1_Pin
};

/* Column order: PA8(col0) PA9(col1) PA10(col2) PA11(col3).
   CubeMX labels: y4=PA8, y3=PA9, y2=PA10, y1=PA11. */
static GPIO_TypeDef *const keypad_col_port[BSP_KEYPAD_COLS] = {
    MKey_y4_GPIO_Port, MKey_y3_GPIO_Port, MKey_y2_GPIO_Port, MKey_y1_GPIO_Port
};
static const uint16_t keypad_col_pin[BSP_KEYPAD_COLS] = {
    MKey_y4_Pin, MKey_y3_Pin, MKey_y2_Pin, MKey_y1_Pin
};

/* Short delay after a row is driven low so the column levels can settle. */
#define KEYPAD_SETTLE_LOOPS  64U

BSP_Keypad_Status_t BSP_Keypad_Init(void)
{
    /* GPIO direction/level is already set by CubeMX MX_GPIO_Init.
       Put rows back to idle-high so no key is reported until scanned. */
    for (uint8_t r = 0; r < BSP_KEYPAD_ROWS; r++)
    {
        HAL_GPIO_WritePin(keypad_row_port[r], keypad_row_pin[r], GPIO_PIN_SET);
    }
    return BSP_KEYPAD_OK;
}

BSP_Keypad_Status_t BSP_Keypad_Scan(uint8_t *p_keycode)
{
    uint8_t r, c;

    if (p_keycode == NULL)
    {
        return BSP_KEYPAD_ERROR;
    }
    *p_keycode = BSP_KEYPAD_NO_KEYCODE;

    for (r = 0; r < BSP_KEYPAD_ROWS; r++)
    {
        HAL_GPIO_WritePin(keypad_row_port[r], keypad_row_pin[r], GPIO_PIN_RESET);

        for (volatile uint32_t d = 0; d < KEYPAD_SETTLE_LOOPS; d++) { }

        for (c = 0; c < BSP_KEYPAD_COLS; c++)
        {
            if (HAL_GPIO_ReadPin(keypad_col_port[c], keypad_col_pin[c]) == GPIO_PIN_RESET)
            {
                *p_keycode = (uint8_t)(r * BSP_KEYPAD_COLS + c);
                HAL_GPIO_WritePin(keypad_row_port[r], keypad_row_pin[r], GPIO_PIN_SET);
                return BSP_KEYPAD_OK;
            }
        }
        HAL_GPIO_WritePin(keypad_row_port[r], keypad_row_pin[r], GPIO_PIN_SET);
    }
    return BSP_KEYPAD_NO_KEY;
}
