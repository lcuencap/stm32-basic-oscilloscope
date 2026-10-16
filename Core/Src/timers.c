#include "timers.h"
#include "gpio_map.h"
#include "encoder_simulator.h"
#include "main.h"
#include "tim.h"
#include "adc.h"

static uint32_t g_tim_tick_us = 1U;
#define APP_ADC_TRIGGER_DELAY_US 20U

/* Debug: verificar trigger ADC */
volatile uint32_t g_dbg_tim2_cnt = 0U;
volatile uint32_t g_dbg_tim2_ccr2 = 0U;
volatile uint32_t g_dbg_tim2_sr = 0U;
volatile uint32_t g_dbg_adc1_sr = 0U;
volatile uint32_t g_dbg_adc3_sr = 0U;
volatile uint32_t g_dbg_adc_trigger_calls = 0U;

static uint32_t Get_APB1_TimerClockHz(void) {
    uint32_t pclk1;
    uint32_t timclk;
    RCC_ClkInitTypeDef clkconfig;
    uint32_t flash_latency;
    uint32_t apb1_div;

    pclk1 = HAL_RCC_GetPCLK1Freq();
    HAL_RCC_GetClockConfig(&clkconfig, &flash_latency);
    apb1_div = clkconfig.APB1CLKDivider;

    if (apb1_div == RCC_HCLK_DIV1) {
        timclk = pclk1;
    } else {
        timclk = 2U * pclk1;
    }

    return timclk;
}

void App_Timers_Init(void) {
    uint32_t timclk;
    uint32_t psc_1mhz;

    timclk = Get_APB1_TimerClockHz();
    psc_1mhz = (timclk / 1000000UL) - 1UL;

    /* TIM2: base temporal en us i font dels compare events cap als ADC. */
    __HAL_TIM_SET_PRESCALER(&htim2, psc_1mhz);
    __HAL_TIM_SET_AUTORELOAD(&htim2, 0xFFFFFFFFUL);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    HAL_TIM_GenerateEvent(&htim2, TIM_EVENTSOURCE_UPDATE);

    /* TIM3: genera un pols de 50 us a OUTPULSE. */
    __HAL_TIM_SET_PRESCALER(&htim3, psc_1mhz);
    __HAL_TIM_SET_AUTORELOAD(&htim3, 49U);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE);

    /* TIM4: rellotge del simulador d'encoder. */
    __HAL_TIM_SET_PRESCALER(&htim4, psc_1mhz);
    __HAL_TIM_SET_AUTORELOAD(&htim4, 499U);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    HAL_TIM_GenerateEvent(&htim4, TIM_EVENTSOURCE_UPDATE);

    HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    HAL_NVIC_SetPriority(TIM4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

void App_TimeBase_Start(void) {
    HAL_TIM_Base_Start(&htim2);
    HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_2);
}

uint32_t App_TimeBase_GetUs(void) {
    return __HAL_TIM_GET_COUNTER(&htim2) * g_tim_tick_us;
}

void App_AdcTrigger_Schedule(void) {
    uint32_t compare_us;

    /* Programem el proper compare de TIM2 CH2 per disparar ADC1 i ADC3 alhora. */
    compare_us = __HAL_TIM_GET_COUNTER(&htim2) + APP_ADC_TRIGGER_DELAY_US;
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC2);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, compare_us);

    /* Debug: capturar estat */
    g_dbg_adc_trigger_calls++;
    g_dbg_tim2_cnt = htim2.Instance->CNT;
    g_dbg_tim2_ccr2 = htim2.Instance->CCR2;
    g_dbg_tim2_sr = htim2.Instance->SR;
    g_dbg_adc1_sr = hadc1.Instance->SR;
    g_dbg_adc3_sr = hadc3.Instance->SR;
}

void App_Pulse_Start50us(void) {
    HAL_GPIO_WritePin(OUTPULSE_PORT, OUTPULSE_PIN_MASK, GPIO_PIN_SET);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    HAL_TIM_Base_Start_IT(&htim3);
}

void App_Sim_Start(void) {
    __HAL_TIM_SET_COUNTER(&htim4, 0U);
    HAL_TIM_Base_Start_IT(&htim4);
}

void App_Sim_Stop(void) {
    HAL_TIM_Base_Stop_IT(&htim4);
}

void App_Sim_SetPeriodUs(uint32_t period_us) {
    if (period_us == 0U) {
        period_us = 1U;
    }

    __HAL_TIM_SET_AUTORELOAD(&htim4, period_us - 1U);
    HAL_TIM_GenerateEvent(&htim4, TIM_EVENTSOURCE_UPDATE);
}

void App_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM3) {
        HAL_TIM_Base_Stop_IT(&htim3);
        HAL_GPIO_WritePin(OUTPULSE_PORT, OUTPULSE_PIN_MASK, GPIO_PIN_RESET);
    } else if (htim->Instance == TIM4) {
        EncoderSim_Step();
    }
}

/* Callback HAL */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    App_TIM_PeriodElapsedCallback(htim);
}
