#include "Oled_Sys.h"
#include "Sw_Adc_Sys.h"
#include "cmsis_os.h"
#include "oled.h"
#include "stdio.h"
#include "main.h"
void Oled_Task_Sys() {
  // Implement the OLED task logic here
    CursorMsg_t msg;
    
    if(state==Statue_NO)
    {
      for (;;)
      {
        if (osMessageQueueGet(cursorHandle, &msg, NULL, 0) == osOK)
        {
          int16_t cx = (int16_t)msg.cursor_x;
          int16_t cy = (int16_t)msg.cursor_y;

          OLED_Clear();

          for (int16_t dy = -3; dy <= 3; dy++)
          {
            for (int16_t dx = -3; dx <= 3; dx++)
            {
              int16_t px = cx + dx;
              int16_t py = cy + dy;
              if (dx*dx + dy*dy <= 9
              && px >= 0 && px < (int16_t)OLED_WIDTH
              && py >= 0 && py < (int16_t)OLED_HEIGHT)
              {
                OLED_DrawPixel((uint8_t)px, (uint8_t)py, OLED_WHITE);
              }
            }
          }

          OLED_SetCursor(0, 56);
          OLED_PrintString("X:");
          OLED_PrintNum((uint32_t)msg.cursor_x, 10);
          OLED_PrintString(" Y:");
          OLED_PrintNum((uint32_t)msg.cursor_y, 10);
          OLED_Display();
          
        }
        osDelay(15);
        
      }
    }
    else if(state==Statue_Yes)
    {
      for (;;)
      {
        
      }

    }
  

}