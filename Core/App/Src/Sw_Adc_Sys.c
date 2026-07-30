#include "Sw_Adc_Sys.h"
#include "cmsis_os.h"
#include "main.h"

extern osMessageQueueId_t cursorHandle;

void Sw_Adc_Task_Sys() {

    uint16_t adc_x, adc_y;
  int16_t cursor_x = 64;
  int16_t cursor_y = 32;
  CursorMsg_t msg;
  static uint8_t prev_btn_state = 1;
  uint8_t button_pressed = 0;
  osDelay(100);

  for (;;)
  {
    if (BSP_ADC_ReadDual(&adc_x, &adc_y) != BSP_ADC_OK)
    {
      osDelay(20);
      continue;
    }

    int16_t dx = (int16_t)adc_x - 2048;
    int16_t dy = (int16_t)adc_y - 2048;

    if (dx > 180 && cursor_x > 0) 
    {
      int16_t step = (dx - 180) / 700 + 1;
      cursor_x -= step;
    }
    if (dx < -180 && cursor_x < 127)  
    {
      int16_t step = (-dx - 180) / 700 + 1;
      cursor_x += step;
    }
    
    if (dy > 180 && cursor_y < 63)  
    {
      int16_t step = (dy - 180) / 700 + 1;
      cursor_y += step;
    }
    if (dy < -180 && cursor_y > 0) 
    {
      int16_t step = (-dy - 180) / 700 + 1;
      cursor_y -= step;
    }
    
    if (cursor_x > 127) cursor_x = 127;
    if (cursor_y > 63) cursor_y = 63;
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;

    uint8_t curr_btn = HAL_GPIO_ReadPin(SW_GPIO_Port, SW_Pin);
    if (curr_btn == 0 && prev_btn_state == 1)
      button_pressed = 1;
    else if (curr_btn == 1 && prev_btn_state == 0)
      button_pressed = 0;
    prev_btn_state = curr_btn;

    msg.cursor_x       = cursor_x;
    msg.cursor_y       = cursor_y;
    msg.button_pressed = button_pressed;
    osMessageQueuePut(cursorHandle, &msg, 0, 0);
    osDelay(20);
  }
}