/**
  ******************************************************************************
  * @file           : Screen_Sys.c
  * @brief          : Turns the OLED off after inactivity and wakes on input
  ******************************************************************************
  */

#include "Screen_Sys.h"
#include "Set_Sys.h"
#include "Log_Sys.h"
#include "oled.h"
#include "main.h"

static uint32_t screen_last_activity = 0U;
static uint8_t screen_off = 0U;

void Screen_Sys_Init(void)
{
    screen_last_activity = HAL_GetTick();
    screen_off = 0U;
    OLED_On();
}

void Screen_Sys_Wake(void)
{
    screen_last_activity = HAL_GetTick();

    if (screen_off)
    {
        OLED_On();
        screen_off = 0U;
        Log_Write(LOG_TYPE_APP, "WAKE");
    }
}

void Screen_Sys_Update(void)
{
    uint32_t timeout;

    if (screen_off) return;

    timeout = Set_Sys_GetScreenTimeoutMs();
    if (timeout > 0U && (HAL_GetTick() - screen_last_activity) >= timeout)
    {
        OLED_Off();
        screen_off = 1U;
        Log_Write(LOG_TYPE_APP, "SLEEP");
    }
}
