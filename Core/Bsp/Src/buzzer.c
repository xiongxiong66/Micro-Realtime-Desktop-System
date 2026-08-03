/**
  ******************************************************************************
  * @file           : buzzer.c
  * @brief          : Active buzzer driver on PB1 (Beep)
  ******************************************************************************
  */

#include "buzzer.h"
#include "main.h"

/* Beep is active low: RESET sounds, SET is silent. */
void Buzzer_Init(void)
{
    HAL_GPIO_WritePin(Beep_GPIO_Port, Beep_Pin, GPIO_PIN_SET);
}

void Buzzer_On(void)
{
    HAL_GPIO_WritePin(Beep_GPIO_Port, Beep_Pin, GPIO_PIN_RESET);
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(Beep_GPIO_Port, Beep_Pin, GPIO_PIN_SET);
}
