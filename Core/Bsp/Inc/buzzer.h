/**
  ******************************************************************************
  * @file           : buzzer.h
  * @brief          : Active buzzer driver header
  ******************************************************************************
  */
#ifndef __BUZZER_H
#define __BUZZER_H

#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_SetFrequency(uint16_t hz);
void Buzzer_SetVolumePermille(uint16_t permille);
void Buzzer_Stop(void);

#endif /* __BUZZER_H */
