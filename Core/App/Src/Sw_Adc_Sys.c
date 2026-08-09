#include "Sw_Adc_Sys.h"
#include "Set_Sys.h"
#include "Screen_Sys.h"
#include "cmsis_os.h"
#include "main.h"

extern osMessageQueueId_t cursorHandle;

uint8_t state = Statue_NO;
volatile uint32_t adc_events = 0U;
volatile uint32_t adc_dropped = 0U;
static uint8_t cursor_suspended = 0U;
static uint8_t prev_state = Statue_NO;

void Cursor_Suspend(void)
{
    if (cursor_suspended == 0U && Sw_AdcHandle != NULL)
    {
        osThreadSuspend(Sw_AdcHandle);
        cursor_suspended = 1U;
    }
}

void Cursor_Resume(void)
{
    if (cursor_suspended != 0U && Sw_AdcHandle != NULL)
    {
        osThreadResume(Sw_AdcHandle);
        cursor_suspended = 0U;
    }
}

void Sw_Adc_Task_Sys() {

    uint16_t adc_x, adc_y;
  int16_t cursor_x = 64;
  int16_t cursor_y = 32;
  CursorMsg_t msg;
  static uint8_t prev_btn_state = 1;
  uint8_t button_pressed = 0;
  uint16_t center_x, center_y;
  uint32_t sum_x = 0U, sum_y = 0U;
  uint8_t i;
  uint8_t moved = 0U;
  uint8_t btn_changed = 0U;
  osDelay(100);

  /* 上电时采样几次摇杆作为中心值，避免静止漂移 */
  for (i = 0; i < 8U; i++)
  {
    uint16_t ax, ay;
    if (BSP_ADC_ReadDual(&ax, &ay) == BSP_ADC_OK)
    {
      sum_x += ax;
      sum_y += ay;
    }
    osDelay(10);
  }
  center_x = (uint16_t)(sum_x / 8U);
  center_y = (uint16_t)(sum_y / 8U);

  for (;;)
  {
    if (state != prev_state)
    {
      if (state == Statue_Yes)
      {
        sum_x = 0U;
        sum_y = 0U;
        for (i = 0U; i < 8U; i++)
        {
          uint16_t ax, ay;
          if (BSP_ADC_ReadDual(&ax, &ay) == BSP_ADC_OK)
          {
            sum_x += ax;
            sum_y += ay;
          }
          osDelay(10);
        }
        center_x = (uint16_t)(sum_x / 8U);
        center_y = (uint16_t)(sum_y / 8U);
        cursor_x = 64;
        cursor_y = 32;
      }
      prev_state = state;
    }

    if (BSP_ADC_ReadDual(&adc_x, &adc_y) != BSP_ADC_OK)
    {
      osDelay(20);
      continue;
    }

    int16_t dx = (int16_t)adc_x - (int16_t)center_x;
    int16_t dy = (int16_t)adc_y - (int16_t)center_y;
    uint8_t sens = Set_Sys_GetSensitivity();
    int16_t divisor = (sens == 0U) ? 900 : (sens == 2U) ? 500 : 700;

    moved = 0U;
    if (dx > 180 && cursor_x > 0) 
    {
      int16_t step = (dx - 180) / divisor + 1;
      cursor_x -= step;
      moved = 1U;
    }
    if (dx < -180 && cursor_x < 127)  
    {
      int16_t step = (-dx - 180) / divisor + 1;
      cursor_x += step;
      moved = 1U;
    }
    
    if (dy > 180 && cursor_y < 63)  
    {
      int16_t step = (dy - 180) / divisor + 1;
      cursor_y += step;
      moved = 1U;
    }
    if (dy < -180 && cursor_y > 0) 
    {
      int16_t step = (-dy - 180) / divisor + 1;
      cursor_y -= step;
      moved = 1U;
    }
    
    if (cursor_x > 127) cursor_x = 127;
    if (cursor_y > 63) cursor_y = 63;
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;

    uint8_t curr_btn = HAL_GPIO_ReadPin(SW_GPIO_Port, SW_Pin);
    btn_changed = 0U;
    if (curr_btn == 0 && prev_btn_state == 1)
    {
      button_pressed = 1;
      btn_changed = 1U;
    }
    else if (curr_btn == 1 && prev_btn_state == 0)
    {
      button_pressed = 0;
      btn_changed = 1U;
    }
    prev_btn_state = curr_btn;

    if (state == Statue_Yes && (moved != 0U || btn_changed != 0U)) Screen_Sys_Wake();

    msg.cursor_x       = cursor_x;
    msg.cursor_y       = cursor_y;
    msg.button_pressed = button_pressed;
    if (osMessageQueuePut(cursorHandle, &msg, 0, 0) == osOK) adc_events++;
    else adc_dropped++;
    osDelay(20);
  }
}
