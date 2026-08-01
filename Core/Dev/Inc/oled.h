#ifndef __OLED_H
#define __OLED_H
#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#include <stdint.h>
#include <string.h>
#include "i2c.h"

#define OLED_I2C_ADDR       0x3CU
#define OLED_WIDTH          128U
#define OLED_HEIGHT         64U
#define OLED_PAGES          (OLED_HEIGHT / 8U)
#define OLED_BUFFER_SIZE    (OLED_WIDTH * OLED_PAGES)
#define OLED_BLACK          0U
#define OLED_WHITE          1U

BSP_I2C_Status_t OLED_Init(void);
void OLED_Display(void);
void OLED_Clear(void);
void OLED_Fill(void);
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_PrintStringColor(uint8_t x, uint8_t y, const char *str, uint8_t color);
void OLED_SetCursor(uint8_t x, uint8_t y);
void OLED_PrintChar(char ch);
void OLED_PrintString(const char *str);
void OLED_PrintNum(uint32_t num, uint8_t base);
void OLED_PrintSignedNum(int32_t num, uint8_t base);
void OLED_On(void);
void OLED_Off(void);
void OLED_Invert(uint8_t enable);

#ifdef __cplusplus
}
#endif
#endif
