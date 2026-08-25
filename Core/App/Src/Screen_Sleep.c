/**
  ******************************************************************************
  * @file           : Screen_Sleep.c
  * @brief          : Tickless idle: enter WFI sleep only while the screen is off
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "Screen_App.h"
#include "main.h"

/* SysTick registers (Cortex-M3) */
#define PORT_SYSTICK_CTRL_REG       (*(volatile uint32_t *)0xE000E010UL)
#define PORT_SYSTICK_LOAD_REG       (*(volatile uint32_t *)0xE000E014UL)
#define PORT_SYSTICK_CURRENT_REG    (*(volatile uint32_t *)0xE000E018UL)
#define PORT_SYSTICK_CLK_BIT        (1UL << 2UL)
#define PORT_SYSTICK_INT_BIT        (1UL << 1UL)
#define PORT_SYSTICK_ENABLE_BIT     (1UL << 0UL)
#define PORT_SYSTICK_COUNT_FLAG_BIT (1UL << 16UL)
#define PORT_MISSED_COUNTS_FACTOR   45UL

#if configUSE_TICKLESS_IDLE == 1

/*
 * 覆盖 FreeRTOS 端口里的 weak 实现：
 * 亮屏时不睡；息屏时用 SysTick 设置睡眠时长进 WFI，
 * 睡眠前停掉 HAL 时间基准 TIM4，醒来后把睡眠时间补回 uwTick。
 */
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements;
    const uint32_t ulCountsForOneTick = configCPU_CLOCK_HZ / configTICK_RATE_HZ;
    const uint32_t ulMaxSuppressedTicks = 0xFFFFFFUL / ulCountsForOneTick;
    const uint32_t ulStoppedCompensation = PORT_MISSED_COUNTS_FACTOR;

    if (Screen_Sys_IsDeepOff() == 0U)
    {
        return;
    }

    if (xExpectedIdleTime > ulMaxSuppressedTicks)
    {
        xExpectedIdleTime = ulMaxSuppressedTicks;
    }

    PORT_SYSTICK_CTRL_REG &= ~PORT_SYSTICK_ENABLE_BIT;

    ulReloadValue = PORT_SYSTICK_CURRENT_REG +
                    (ulCountsForOneTick * (xExpectedIdleTime - 1UL));
    if (ulReloadValue > ulStoppedCompensation)
    {
        ulReloadValue -= ulStoppedCompensation;
    }

    __asm volatile("cpsid i" ::: "memory");
    __asm volatile("dsb");
    __asm volatile("isb");

    if (eTaskConfirmSleepModeStatus() == eAbortSleep)
    {
        PORT_SYSTICK_LOAD_REG = PORT_SYSTICK_CURRENT_REG;
        PORT_SYSTICK_CTRL_REG |= PORT_SYSTICK_ENABLE_BIT;
        PORT_SYSTICK_LOAD_REG = ulCountsForOneTick - 1UL;
        __asm volatile("cpsie i" ::: "memory");
        return;
    }

    PORT_SYSTICK_LOAD_REG = ulReloadValue;
    PORT_SYSTICK_CURRENT_REG = 0UL;
    PORT_SYSTICK_CTRL_REG |= PORT_SYSTICK_ENABLE_BIT;

    /* 停掉 TIM4 更新中断，避免它每 1ms 唤醒 CPU */
    HAL_SuspendTick();

    __asm volatile("dsb" ::: "memory");
    __asm volatile("wfi");
    __asm volatile("isb");

    /* 保持中断关闭先完成 tick 记账。CMSIS-RTOS2 的 SysTick_Handler 会读
       SysTick->CTRL 清 COUNTFLAG，如果先放行中断，这里会误判为提前唤醒，
       导致每次睡眠只补 1 个 tick，任务永远等不到 osDelay 到期。 */
    PORT_SYSTICK_CTRL_REG = (PORT_SYSTICK_CLK_BIT | PORT_SYSTICK_INT_BIT);

    if ((PORT_SYSTICK_CTRL_REG & PORT_SYSTICK_COUNT_FLAG_BIT) != 0UL)
    {
        uint32_t ulCalculatedLoadValue;

        ulCalculatedLoadValue = (ulCountsForOneTick - 1UL) -
                                (ulReloadValue - PORT_SYSTICK_CURRENT_REG);
        if ((ulCalculatedLoadValue < ulStoppedCompensation) ||
            (ulCalculatedLoadValue > ulCountsForOneTick))
        {
            ulCalculatedLoadValue = ulCountsForOneTick - 1UL;
        }
        PORT_SYSTICK_LOAD_REG = ulCalculatedLoadValue;
        ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
    }
    else
    {
        ulCompletedSysTickDecrements = (xExpectedIdleTime * ulCountsForOneTick) -
                                       PORT_SYSTICK_CURRENT_REG;
        ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulCountsForOneTick;
        PORT_SYSTICK_LOAD_REG = ((ulCompleteTickPeriods + 1UL) * ulCountsForOneTick) -
                                ulCompletedSysTickDecrements;
    }

    PORT_SYSTICK_CURRENT_REG = 0UL;
    PORT_SYSTICK_CTRL_REG |= PORT_SYSTICK_ENABLE_BIT;
    vTaskStepTick(ulCompleteTickPeriods);
    PORT_SYSTICK_LOAD_REG = ulCountsForOneTick - 1UL;

    /* 补回 HAL tick，恢复 TIM4 中断 */
    uwTick += ulCompleteTickPeriods;
    HAL_ResumeTick();

    __asm volatile("cpsie i" ::: "memory");
    __asm volatile("dsb");
    __asm volatile("isb");
}

#endif /* configUSE_TICKLESS_IDLE */
