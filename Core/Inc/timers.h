#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

void App_Timers_Init(void);

void App_TimeBase_Start(void);
uint32_t App_TimeBase_GetUs(void);
void App_AdcTrigger_Schedule(void);

void App_Pulse_Start50us(void);

void App_Sim_Start(void);
void App_Sim_Stop(void);
void App_Sim_SetPeriodUs(uint32_t period_us);

/* callback centralitzada per timers */
void App_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

#endif
