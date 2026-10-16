#include "encoder_simulator.h"
#include "gpio_map.h"
#include "timers.h"
#include "main.h"
#include "encoder.h"

#define IDX_PULSE_US 1000U

static volatile uint8_t g_running = 0U;
static volatile uint8_t g_quad_state = 0U;
static volatile uint8_t g_idx_active = 0U;
static volatile uint32_t g_idx_start_us = 0U;
static volatile int8_t g_direction = 1;
volatile uint8_t g_sim_force_reverse = 0U;
volatile uint32_t g_sim_start_state = 0U;
volatile uint32_t g_sim_reverse_at_edge = 0U;
volatile uint8_t g_sim_auto_reverse_enable = 0U;

/* Seqüència dreta -> esquerra: 00 -> 10 -> 11 -> 01 -> 00 */
static const uint8_t quad_table[4][2] = {
    {0, 0},
    {1, 0},
    {1, 1},
    {0, 1}
};

static void GPIO_Write(GPIO_TypeDef *port, uint16_t pin, uint8_t value) {
    HAL_GPIO_WritePin(port, pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void EncoderSim_Init(void) {
    g_running = 0U;
    g_quad_state = 0U;
    g_idx_active = 0U;
    g_idx_start_us = 0U;
    g_direction = 1;
    g_sim_force_reverse = 0U;
    g_sim_start_state = 0U;
    g_sim_reverse_at_edge = 0U;
    g_sim_auto_reverse_enable = 0U;

    GPIO_Write(OUTSIMA_PORT, OUTSIMA_PIN_MASK, 0U);
    GPIO_Write(OUTSIMB_PORT, OUTSIMB_PIN_MASK, 0U);
    GPIO_Write(OUTSIMIDX_PORT, OUTSIMIDX_PIN_MASK, 0U);
}

void EncoderSim_StartBand(void) {
    g_running = 1U;
    g_quad_state = (uint8_t)(g_sim_start_state & 0x03U);
    g_direction = 1;

    GPIO_Write(OUTSIMA_PORT, OUTSIMA_PIN_MASK, quad_table[g_quad_state][0]);
    GPIO_Write(OUTSIMB_PORT, OUTSIMB_PIN_MASK, quad_table[g_quad_state][1]);

    GPIO_Write(OUTSIMIDX_PORT, OUTSIMIDX_PIN_MASK, 1U);
    g_idx_start_us = App_TimeBase_GetUs();
    g_idx_active = 1U;

    App_Sim_Start();
}

void EncoderSim_Stop(void) {
    g_running = 0U;
    g_idx_active = 0U;

    App_Sim_Stop();

    GPIO_Write(OUTSIMIDX_PORT, OUTSIMIDX_PIN_MASK, 0U);
    GPIO_Write(OUTSIMA_PORT, OUTSIMA_PIN_MASK, 0U);
    GPIO_Write(OUTSIMB_PORT, OUTSIMB_PIN_MASK, 0U);
}

void EncoderSim_SetSpeed_mm_s(uint32_t speed_mm_s) {
    uint32_t period_us;

    if (speed_mm_s == 0U) {
        speed_mm_s = 100U;
    }

    period_us = 50000U / speed_mm_s;
    if (period_us == 0U) {
        period_us = 1U;
    }

    App_Sim_SetPeriodUs(period_us);
}

void EncoderSim_Task(void) {
    uint32_t now_us;

    if (g_idx_active == 0U) {
        return;
    }

    now_us = App_TimeBase_GetUs();

    if ((now_us - g_idx_start_us) >= IDX_PULSE_US) {
        GPIO_Write(OUTSIMIDX_PORT, OUTSIMIDX_PIN_MASK, 0U);
        g_idx_active = 0U;
    }
}

uint8_t EncoderSim_IsRunning(void) {
    return g_running;
}

void EncoderSim_Step(void) {
    const EncoderStatus *enc;

    if (!g_running) {
        return;
    }

    enc = Encoder_GetStatus();

    if ((g_sim_auto_reverse_enable != 0U) &&
        (enc->state == ENC_PRINTING) &&
        (enc->edge_count >= g_sim_reverse_at_edge)) {
        g_sim_force_reverse = 1U;
    }

    if (g_sim_force_reverse != 0U) {
        g_direction = -1;
    } else {
        g_direction = 1;
    }

    if (g_direction > 0) {
        g_quad_state++;
        if (g_quad_state >= 4U) {
            g_quad_state = 0U;
        }
    } else {
        if (g_quad_state == 0U) {
            g_quad_state = 3U;
        } else {
            g_quad_state--;
        }
    }

    GPIO_Write(OUTSIMA_PORT, OUTSIMA_PIN_MASK, quad_table[g_quad_state][0]);
    GPIO_Write(OUTSIMB_PORT, OUTSIMB_PIN_MASK, quad_table[g_quad_state][1]);
}
