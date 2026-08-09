/**
  ******************************************************************************
  * @file           : buzzer.c
  * @brief          : Passive buzzer driver on PB1 (TIM3_CH4)
  ******************************************************************************
  */

#include "buzzer.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;

#define BUZZER_PWM_CLK_HZ   1000000U
#define BUZZER_PRESCALER    71U

static uint32_t buzzer_period = 0U;
static uint16_t buzzer_duty_permille = 500U;

void Buzzer_Init(void)
{
    buzzer_period = 0U;
    buzzer_duty_permille = 500U;
    Buzzer_Stop();
}

void Buzzer_On(void)
{
    Buzzer_SetFrequency(1000U);
}

void Buzzer_Off(void)
{
    Buzzer_Stop();
}

void Buzzer_SetFrequency(uint16_t hz)
{
    uint32_t period;

    if (hz == 0U || buzzer_duty_permille == 0U)
    {
        Buzzer_Stop();
        return;
    }

    period = (BUZZER_PWM_CLK_HZ / hz) - 1U;
    if (period > 0xFFFFU) period = 0xFFFFU;
    buzzer_period = period;

    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_4);
    __HAL_TIM_SET_PRESCALER(&htim3, BUZZER_PRESCALER);
    __HAL_TIM_SET_AUTORELOAD(&htim3, period);
    {
        uint32_t compare = (uint32_t)period * buzzer_duty_permille / 1000U;
        if (compare == 0U) compare = 1U;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, compare);
    }
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

void Buzzer_SetVolumePermille(uint16_t permille)
{
    if (permille > 1000U) permille = 1000U;

    buzzer_duty_permille = permille;
    if (buzzer_period != 0U)
    {
        uint32_t compare = (uint32_t)buzzer_period * buzzer_duty_permille / 1000U;
        if (compare == 0U && buzzer_duty_permille > 0U) compare = 1U;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, compare);
    }
}

void Buzzer_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_4);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0U);
    buzzer_period = 0U;
}
