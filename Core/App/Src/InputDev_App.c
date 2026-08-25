/**
  ******************************************************************************
  * @file           : InputDev_App.c
  * @brief          : Input device connection detection (joystick)
  ******************************************************************************
  */

#include "InputDev_App.h"
#include "Log_App.h"

#define INPUTDEV_JOY_AVG_SAMPLES    100U
#define INPUTDEV_JOY_BL_MIN_OFFSET  500U

static volatile uint8_t input_joy_ok = 1U;
static uint8_t input_joy_prev = 1U;
static uint16_t input_x_buf[INPUTDEV_JOY_AVG_SAMPLES] = {0U};
static uint16_t input_y_buf[INPUTDEV_JOY_AVG_SAMPLES] = {0U};
static uint8_t input_buf_idx = 0U;
static uint32_t input_sum_x = 0U;
static uint32_t input_sum_y = 0U;
static uint32_t input_sample_count = 0U;

uint8_t InputDev_IsConnected(void)
{
    return input_joy_ok;
}

/* 平均值判断：两轴持续偏向左下角（相对中心偏移超过阈值）才判定断开 */
void InputDev_MonitorSample(uint16_t x, uint16_t y, uint16_t center_x, uint16_t center_y)
{
    uint32_t avg_x, avg_y;

    input_sum_x += (uint32_t)x - input_x_buf[input_buf_idx];
    input_x_buf[input_buf_idx] = x;
    input_sum_y += (uint32_t)y - input_y_buf[input_buf_idx];
    input_y_buf[input_buf_idx] = y;

    input_buf_idx++;
    if (input_buf_idx >= INPUTDEV_JOY_AVG_SAMPLES) input_buf_idx = 0U;
    if (input_sample_count < INPUTDEV_JOY_AVG_SAMPLES) input_sample_count++;

    avg_x = input_sum_x / input_sample_count;
    avg_y = input_sum_y / input_sample_count;

    if ((int32_t)avg_x - (int32_t)center_x > (int32_t)INPUTDEV_JOY_BL_MIN_OFFSET &&
        (int32_t)avg_y - (int32_t)center_y > (int32_t)INPUTDEV_JOY_BL_MIN_OFFSET)
    {
        input_joy_ok = 0U;
    }
    else
    {
        input_joy_ok = 1U;
    }

    if (input_joy_ok != input_joy_prev)
    {
        input_joy_prev = input_joy_ok;
        if (input_joy_ok != 0U) Log_Write(LOG_TYPE_APP, "JOY ON");
        else Log_Write(LOG_TYPE_ERROR, "JOY OFF");
    }
}
