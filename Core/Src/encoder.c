#include "encoder.h"
#include "main.h"
#include "timers.h"
#include "gpio.h"

#define PRINT_DISTANCE_MM 600U
#define EOP_EDGE_COUNT    12000U

static EncoderStatus g_enc;

/* Variables g_dbg_* per inspeccionar l'estat pel debugger. */
volatile uint32_t g_dbg_avg_speed_mm_s = 0U;
volatile uint32_t g_dbg_edge_count = 0U;
volatile uint32_t g_dbg_state = 0U;
volatile uint32_t g_dbg_error = 0U;
volatile uint32_t g_dbg_eop = 0U;
volatile uint32_t g_dbg_enc_index_events = 0U;
volatile uint32_t g_dbg_enc_edge_events_a = 0U;
volatile uint32_t g_dbg_enc_edge_events_b = 0U;
volatile uint32_t g_dbg_enc_dir_rtl_count = 0U;
volatile uint32_t g_dbg_enc_dir_ltr_count = 0U;
volatile uint32_t g_dbg_enc_wrong_dir_errors = 0U;
volatile uint32_t g_dbg_enc_eop_events = 0U;
volatile uint32_t g_dbg_enc_last_edge_us = 0U;
volatile uint32_t g_dbg_enc_prev_edge_us = 0U;
volatile uint32_t g_dbg_enc_edge_dt_us = 0U;


volatile uint32_t g_dbg_t_start_us = 0U;
volatile uint32_t g_dbg_t_end_us = 0U;
volatile uint32_t g_dbg_dt_us = 0U;

static EncDirection Encoder_DecodeDirection(uint8_t is_channel_a, uint8_t a_level, uint8_t b_level) {
    /* Decodifica la direccio a partir del canal que ha canviat i l'estat A/B. */
    if (is_channel_a) {
        if (a_level != 0U) {
            return (b_level == 0U) ? DIR_RTL : DIR_LTR;
        } else {
            return (b_level == 0U) ? DIR_LTR : DIR_RTL;
        }
    } else {
        if (b_level != 0U) {
            return (a_level == 0U) ? DIR_LTR : DIR_RTL;
        } else {
            return (a_level == 0U) ? DIR_RTL : DIR_LTR;
        }
    }
}

static void Encoder_UpdateDebugVars(void) {
    g_dbg_edge_count = g_enc.edge_count;
    g_dbg_state = g_enc.state;
    g_dbg_error = g_enc.error;
    g_dbg_eop = g_enc.eop;
}

static void Encoder_ResetOutputs(void) {
    HAL_GPIO_WritePin(OUTEOP_GPIO_Port, OUTEOP_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OUTERR_GPIO_Port, OUTERR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OUTPULSE_GPIO_Port, OUTPULSE_Pin, GPIO_PIN_RESET);
}

static void Encoder_GeneratePulse(void) {
    /* Cada flanc valid ha de generar un pols curt de sortida. */
    App_Pulse_Start50us();
}

static void Encoder_FinishOk(void) {
    uint32_t dt_us;

    g_enc.t_end_us = App_TimeBase_GetUs();
    dt_us = g_enc.t_end_us - g_enc.t_start_us;

    g_dbg_t_start_us = g_enc.t_start_us;
    g_dbg_t_end_us = g_enc.t_end_us;
    g_dbg_dt_us = dt_us;

    if (dt_us > 0U) {
        uint64_t avg_speed_x10;

        avg_speed_x10 = ((uint64_t)PRINT_DISTANCE_MM * 10000000ULL) / (uint64_t)dt_us;
        g_enc.avg_speed_mm_s = (uint32_t)((avg_speed_x10 + 5ULL) / 10ULL);
        g_dbg_avg_speed_mm_s = g_enc.avg_speed_mm_s;
    } else {
        g_enc.avg_speed_mm_s = 0U;
        g_dbg_avg_speed_mm_s = 0U;
    }

    g_enc.eop = true;
    g_enc.error = false;
    g_enc.state = ENC_DONE;

    HAL_GPIO_WritePin(OUTEOP_GPIO_Port, OUTEOP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OUTERR_GPIO_Port, OUTERR_Pin, GPIO_PIN_RESET);

    Encoder_UpdateDebugVars();
}

static void Encoder_SetError(void) {
    g_enc.error = true;
    g_enc.eop = false;
    g_enc.avg_speed_mm_s = 0U;
    g_enc.t_end_us = 0U;
    g_enc.state = ENC_ERROR;
    g_dbg_avg_speed_mm_s = 0U;

    HAL_GPIO_WritePin(OUTERR_GPIO_Port, OUTERR_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OUTEOP_GPIO_Port, OUTEOP_Pin, GPIO_PIN_RESET);

    Encoder_UpdateDebugVars();
}

static void Encoder_HandleEdge(uint8_t is_channel_a) {
    uint8_t a_level;
    uint8_t b_level;
    EncDirection dir;
    uint32_t now_us;

    HAL_GPIO_WritePin(DBG_ISR_GPIO_Port, DBG_ISR_Pin, GPIO_PIN_SET);

    if (g_enc.state != ENC_PRINTING) {
        HAL_GPIO_WritePin(DBG_ISR_GPIO_Port, DBG_ISR_Pin, GPIO_PIN_RESET);
        return;
    }

    a_level = (uint8_t)HAL_GPIO_ReadPin(INA_GPIO_Port, INA_Pin);
    b_level = (uint8_t)HAL_GPIO_ReadPin(INB_GPIO_Port, INB_Pin);

    dir = Encoder_DecodeDirection(is_channel_a, a_level, b_level);
    g_enc.last_dir = dir;
    if (dir == DIR_RTL) {
        g_dbg_enc_dir_rtl_count++;
    } else if (dir == DIR_LTR) {
        g_dbg_enc_dir_ltr_count++;
    }

    now_us = App_TimeBase_GetUs();
    g_dbg_enc_prev_edge_us = g_dbg_enc_last_edge_us;
    g_dbg_enc_last_edge_us = now_us;
    if (g_dbg_enc_prev_edge_us != 0U) {
        g_dbg_enc_edge_dt_us = g_dbg_enc_last_edge_us - g_dbg_enc_prev_edge_us;
    }

    Encoder_GeneratePulse();

    /* La practica nomes accepta moviment de dreta a esquerra. */
    if (dir != DIR_RTL) {
        g_dbg_enc_wrong_dir_errors++;
        Encoder_SetError();
        HAL_GPIO_WritePin(DBG_ISR_GPIO_Port, DBG_ISR_Pin, GPIO_PIN_RESET);
        return;
    }

    g_enc.edge_count++;

    /* 12000 flancs equivalen a 600 mm recorreguts. */
    if (g_enc.edge_count >= EOP_EDGE_COUNT) {
        g_dbg_enc_eop_events++;
        Encoder_FinishOk();
    } else {
        Encoder_UpdateDebugVars();
    }

    HAL_GPIO_WritePin(DBG_ISR_GPIO_Port, DBG_ISR_Pin, GPIO_PIN_RESET);
}

void Encoder_Init(void) {
    g_enc.state = ENC_IDLE;
    g_enc.edge_count = 0U;
    g_enc.t_start_us = 0U;
    g_enc.t_end_us = 0U;
    g_enc.avg_speed_mm_s = 0U;
    g_enc.last_dir = DIR_UNKNOWN;
    g_enc.idx_seen = false;
    g_enc.eop = false;
    g_enc.error = false;
    g_dbg_avg_speed_mm_s = 0U;

    g_dbg_t_start_us = 0U;
    g_dbg_t_end_us = 0U;
    g_dbg_dt_us = 0U;

    Encoder_ResetOutputs();
    Encoder_UpdateDebugVars();
}

void Encoder_OnIndex(void) {
	g_dbg_enc_index_events++;
	if (g_enc.state == ENC_PRINTING) {
	        return;
	}

    g_enc.state = ENC_PRINTING;
    g_enc.edge_count = 0U;
    g_enc.t_start_us = App_TimeBase_GetUs();
    g_enc.t_end_us = 0U;
    g_enc.avg_speed_mm_s = 0U;
    g_enc.last_dir = DIR_UNKNOWN;
    g_enc.idx_seen = true;
    g_enc.eop = false;
    g_enc.error = false;
    g_dbg_avg_speed_mm_s = 0U;

    g_dbg_t_start_us = 0U;
    g_dbg_t_end_us = 0U;
    g_dbg_dt_us = 0U;

    Encoder_ResetOutputs();
    Encoder_UpdateDebugVars();
}

void Encoder_OnEdgeA(void) {
    g_dbg_enc_edge_events_a++;
    Encoder_HandleEdge(1U);
}

void Encoder_OnEdgeB(void) {
    g_dbg_enc_edge_events_b++;
    Encoder_HandleEdge(0U);
}

const EncoderStatus *Encoder_GetStatus(void) {
    return &g_enc;
}
