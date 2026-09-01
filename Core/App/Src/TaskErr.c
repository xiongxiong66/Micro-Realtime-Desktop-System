/**
  ******************************************************************************
  * @file           : TaskErr.c
  * @brief          : Stack overflow / malloc failed hooks with flash log
  ******************************************************************************
  */

#include "TaskErr.h"
#include "Log_App.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "sflash.h"
#include "w25q64.h"
#include <string.h>

#define TASK_ERR_SECTOR      1U    /* W25Q64 预留扇区，不占用户数据 */
#define TASK_ERR_MAGIC       0x45525231UL  /* "ERR1" */
#define TASK_ERR_TYPE_STACK  1U
#define TASK_ERR_TYPE_MALLOC 2U
#define TASK_ERR_RECORD_SIZE 32U
#define TASK_ERR_NAME_LEN    16U

static uint8_t task_err_record[TASK_ERR_RECORD_SIZE];

static void task_err_store(uint8_t type, const char *name)
{
    uint32_t addr = (uint32_t)TASK_ERR_SECTOR * SFLASH_SECTOR_SIZE;

    memset(task_err_record, 0, sizeof(task_err_record));
    task_err_record[0] = (uint8_t)(TASK_ERR_MAGIC & 0xFFU);
    task_err_record[1] = (uint8_t)((TASK_ERR_MAGIC >> 8U) & 0xFFU);
    task_err_record[2] = (uint8_t)((TASK_ERR_MAGIC >> 16U) & 0xFFU);
    task_err_record[3] = (uint8_t)((TASK_ERR_MAGIC >> 24U) & 0xFFU);
    task_err_record[4] = type;
    if (name != NULL)
    {
        strncpy((char *)&task_err_record[5], name, TASK_ERR_NAME_LEN - 1U);
    }

    /* SPI 片选被占用说明 Log 任务正在写 Flash，跳过写入直接复位 */
    if (HAL_GPIO_ReadPin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin) == GPIO_PIN_SET)
    {
        (void)BSP_W25Q64_Write(addr, task_err_record, sizeof(task_err_record));
    }

    NVIC_SystemReset();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    task_err_store(TASK_ERR_TYPE_STACK, pcTaskName);
}

void vApplicationMallocFailedHook(void)
{
    task_err_store(TASK_ERR_TYPE_MALLOC, NULL);
}

void TaskErr_Init(void)
{
    uint8_t buf[TASK_ERR_RECORD_SIZE];
    uint32_t addr = (uint32_t)TASK_ERR_SECTOR * SFLASH_SECTOR_SIZE;
    char text[LOG_TEXT_LEN];

    if (BSP_W25Q64_Read(addr, buf, sizeof(buf)) != BSP_W25Q64_OK) return;

    if (buf[0] != (uint8_t)(TASK_ERR_MAGIC & 0xFFU) ||
        buf[1] != (uint8_t)((TASK_ERR_MAGIC >> 8U) & 0xFFU) ||
        buf[2] != (uint8_t)((TASK_ERR_MAGIC >> 16U) & 0xFFU) ||
        buf[3] != (uint8_t)((TASK_ERR_MAGIC >> 24U) & 0xFFU))
    {
        return;
    }

    if (buf[4] == TASK_ERR_TYPE_STACK)
    {
        char name[TASK_ERR_NAME_LEN + 1U];

        memcpy(name, &buf[5], TASK_ERR_NAME_LEN);
        name[TASK_ERR_NAME_LEN] = '\0';
        memcpy(text, "STK ", 4U);
        text[4] = '\0';
        strncat(text, name, LOG_TEXT_LEN - 1U - 4U);
    }
    else
    {
        strncpy(text, "MALLOC FAIL", LOG_TEXT_LEN - 1U);
    }

    (void)BSP_W25Q64_EraseSector(addr);
    Log_Write(LOG_TYPE_ERROR, text);
}
