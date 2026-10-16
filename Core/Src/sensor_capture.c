#include "sensor_capture.h"
#include "adc.h"
#include "memory_map.h"
#include "main.h"
#include "timers.h"
#include "app_hw_patch.h"

#define SENSOR_DMA_DB_BUFFER_COUNT 2U
#define SENSOR_DMA_DB_CHUNK        100U
#define SENSOR_DMA_DB_BLOCKS       (SENSOR_SAMPLE_COUNT / SENSOR_DMA_DB_CHUNK)

#if ((SENSOR_SAMPLE_COUNT % SENSOR_DMA_DB_CHUNK) != 0U)
#error "SENSOR_SAMPLE_COUNT ha de ser multiple de SENSOR_DMA_DB_CHUNK"
#endif

uint16_t g_front_sensor_raw[SENSOR_SAMPLE_COUNT];
uint16_t g_rear_sensor_raw[SENSOR_SAMPLE_COUNT];

static uint16_t g_front_sensor_dma_stage[SENSOR_DMA_DB_BUFFER_COUNT][SENSOR_DMA_DB_CHUNK];
static uint16_t g_rear_sensor_dma_stage[SENSOR_DMA_DB_BUFFER_COUNT][SENSOR_DMA_DB_CHUNK];

/* Estat de captura i patro del primer flanc valid despres d'IDX. */
static volatile SensorCaptureState g_state = SENSOR_CAP_IDLE;
static volatile uint32_t g_sample_index = 0U;
static volatile uint8_t g_first_edge_pin = 0U;    /* 0 = A, 1 = B */
static volatile uint8_t g_first_edge_level = 0U;  /* 1 = rising, 0 = falling */
static volatile uint8_t g_processing_pending = 0U;
static volatile uint8_t g_dma_running = 0U;
static volatile uint8_t g_front_dma_done = 0U;
static volatile uint8_t g_rear_dma_done = 0U;

static volatile uint32_t g_front_full_count = 0U;
static volatile uint32_t g_rear_full_count = 0U;
static volatile uint32_t g_consumed_full_pairs = 0U;

/* Variables g_dbg_* per observacio de la captura des del debugger. */
volatile uint32_t g_dbg_sensor_cap_state = 0U;
volatile uint32_t g_dbg_sensor_sample_count = 0U;
volatile uint32_t g_dbg_sensor_processing_pending = 0U;
volatile uint32_t g_dbg_front_last_raw = 0U;
volatile uint32_t g_dbg_rear_last_raw = 0U;
volatile uint32_t g_dbg_cap_index_events = 0U;
volatile uint32_t g_dbg_cap_first_lock_events = 0U;
volatile uint32_t g_dbg_cap_locked_pin = 0U;
volatile uint32_t g_dbg_cap_locked_level = 0U;
volatile uint32_t g_dbg_cap_edge_match_count = 0U;
volatile uint32_t g_dbg_cap_edge_mismatch_count = 0U;
volatile uint32_t g_dbg_cap_trigger_count = 0U;
volatile uint32_t g_dbg_cap_last_trigger_us = 0U;
volatile uint32_t g_dbg_cap_prev_trigger_us = 0U;
volatile uint32_t g_dbg_cap_trigger_dt_us = 0U;
volatile uint32_t g_dbg_cap_trigger_dt_min_us = 0xFFFFFFFFUL;
volatile uint32_t g_dbg_cap_trigger_dt_max_us = 0U;
volatile uint32_t g_dbg_cap_dma_half_adc1_count = 0U;
volatile uint32_t g_dbg_cap_dma_half_adc3_count = 0U;
volatile uint32_t g_dbg_cap_dma_full_adc1_count = 0U;
volatile uint32_t g_dbg_cap_dma_full_adc3_count = 0U;
volatile uint32_t g_dbg_cap_dma_pair_half_count = 0U;
volatile uint32_t g_dbg_cap_dma_pair_full_count = 0U;
volatile uint32_t g_dbg_cap_dma_pair_error_count = 0U;
volatile uint32_t g_dbg_cap_dma_spurious_cb_count = 0U;
volatile uint32_t g_dbg_cap_dma_start_error_count = 0U;
volatile uint32_t g_dbg_cap_raw_done_us = 0U;
volatile uint32_t g_dbg_cap_processing_pending_set_count = 0U;
volatile uint32_t g_dbg_cap_processing_pending_clear_count = 0U;

static volatile uint16_t g_front_last_committed_raw = 0U;
static volatile uint16_t g_rear_last_committed_raw = 0U;

static void SensorCapture_ClearRawBuffers(void) {
    uint32_t i;

    for (i = 0U; i < SENSOR_SAMPLE_COUNT; i++) {
        g_front_sensor_raw[i] = 0U;
        g_rear_sensor_raw[i] = 0U;
    }
}

static void SensorCapture_ClearStageBuffers(void) {
    uint32_t block;
    uint32_t sample;

    for (block = 0U; block < SENSOR_DMA_DB_BUFFER_COUNT; block++) {
        for (sample = 0U; sample < SENSOR_DMA_DB_CHUNK; sample++) {
            g_front_sensor_dma_stage[block][sample] = 0U;
            g_rear_sensor_dma_stage[block][sample] = 0U;
        }
    }
}

static void SensorCapture_ResetDmaProgress(void) {
    g_front_dma_done = 0U;
    g_rear_dma_done = 0U;
    g_front_full_count = 0U;
    g_rear_full_count = 0U;
    g_consumed_full_pairs = 0U;
    g_front_last_committed_raw = 0U;
    g_rear_last_committed_raw = 0U;
}

static void SensorCapture_ResetRuntimeState(void) {
    g_state = SENSOR_CAP_IDLE;
    g_sample_index = 0U;
    g_first_edge_pin = 0U;
    g_first_edge_level = 0U;
    g_processing_pending = 0U;
    g_dma_running = 0U;
    SensorCapture_ResetDmaProgress();
}

static void SensorCapture_ResetDebugCounters(void) {
    g_dbg_cap_index_events = 0U;
    g_dbg_cap_first_lock_events = 0U;
    g_dbg_cap_locked_pin = 0U;
    g_dbg_cap_locked_level = 0U;
    g_dbg_cap_edge_match_count = 0U;
    g_dbg_cap_edge_mismatch_count = 0U;
    g_dbg_cap_trigger_count = 0U;
    g_dbg_cap_last_trigger_us = 0U;
    g_dbg_cap_prev_trigger_us = 0U;
    g_dbg_cap_trigger_dt_us = 0U;
    g_dbg_cap_trigger_dt_min_us = 0xFFFFFFFFUL;
    g_dbg_cap_trigger_dt_max_us = 0U;
    g_dbg_cap_dma_half_adc1_count = 0U;
    g_dbg_cap_dma_half_adc3_count = 0U;
    g_dbg_cap_dma_full_adc1_count = 0U;
    g_dbg_cap_dma_full_adc3_count = 0U;
    g_dbg_cap_dma_pair_half_count = 0U;
    g_dbg_cap_dma_pair_full_count = 0U;
    g_dbg_cap_dma_pair_error_count = 0U;
    g_dbg_cap_dma_spurious_cb_count = 0U;
    g_dbg_cap_dma_start_error_count = 0U;
    g_dbg_cap_raw_done_us = 0U;
    g_dbg_cap_processing_pending_set_count = 0U;
    g_dbg_cap_processing_pending_clear_count = 0U;
}

static void SensorCapture_ResetDebugAcquisitionCounters(void) {
    g_dbg_cap_trigger_count = 0U;
    g_dbg_cap_last_trigger_us = 0U;
    g_dbg_cap_prev_trigger_us = 0U;
    g_dbg_cap_trigger_dt_us = 0U;
    g_dbg_cap_trigger_dt_min_us = 0xFFFFFFFFUL;
    g_dbg_cap_trigger_dt_max_us = 0U;
    g_dbg_cap_dma_half_adc1_count = 0U;
    g_dbg_cap_dma_half_adc3_count = 0U;
    g_dbg_cap_dma_full_adc1_count = 0U;
    g_dbg_cap_dma_full_adc3_count = 0U;
    g_dbg_cap_dma_pair_half_count = 0U;
    g_dbg_cap_dma_pair_full_count = 0U;
    g_dbg_cap_dma_pair_error_count = 0U;
    g_dbg_cap_dma_spurious_cb_count = 0U;
    g_dbg_cap_raw_done_us = 0U;
}

static void SensorCapture_UpdateDebug(void) {
    g_dbg_sensor_cap_state = (uint32_t)g_state;
    g_dbg_sensor_sample_count = g_sample_index;
    g_dbg_sensor_processing_pending = g_processing_pending;
    g_dbg_front_last_raw = g_front_last_committed_raw;
    g_dbg_rear_last_raw = g_rear_last_committed_raw;
}

static uint8_t SensorCapture_GetPinLevel(uint16_t pin) {
    if (pin == INA_Pin) {
        return (uint8_t)HAL_GPIO_ReadPin(INA_GPIO_Port, INA_Pin);
    }

    return (uint8_t)HAL_GPIO_ReadPin(INB_GPIO_Port, INB_Pin);
}

static void SensorCapture_LockFirstEdge(uint8_t pin_id, uint8_t level) {
    g_first_edge_pin = pin_id;
    g_first_edge_level = level;
    g_dbg_cap_locked_pin = pin_id;
    g_dbg_cap_locked_level = level;
    g_dbg_cap_first_lock_events++;
    g_state = SENSOR_CAP_ACQUIRING;
}

static uint8_t SensorCapture_IsMatchingEdge(uint8_t pin_id, uint8_t level) {
    return (uint8_t)((pin_id == g_first_edge_pin) && (level == g_first_edge_level));
}

static void SensorCapture_RecordTriggerEvent(void) {
    uint32_t now_us;
    uint32_t dt_us;

    now_us = App_TimeBase_GetUs();
    g_dbg_cap_prev_trigger_us = g_dbg_cap_last_trigger_us;
    g_dbg_cap_last_trigger_us = now_us;
    g_dbg_cap_trigger_count++;

    if (g_dbg_cap_prev_trigger_us != 0U) {
        dt_us = g_dbg_cap_last_trigger_us - g_dbg_cap_prev_trigger_us;
        g_dbg_cap_trigger_dt_us = dt_us;

        if (dt_us < g_dbg_cap_trigger_dt_min_us) {
            g_dbg_cap_trigger_dt_min_us = dt_us;
        }

        if (dt_us > g_dbg_cap_trigger_dt_max_us) {
            g_dbg_cap_trigger_dt_max_us = dt_us;
        }
    }
}

static void SensorCapture_StopDma(void) {
    (void)HAL_ADC_Stop_DMA(&hadc3);
    (void)HAL_ADC_Stop_DMA(&hadc1);
    g_dma_running = 0U;
}

static void SensorCapture_CopyCommittedBlock(uint32_t block_index) {
    uint32_t dst_offset;
    uint32_t sample_index;
    uint32_t mem_slot;

    dst_offset = block_index * SENSOR_DMA_DB_CHUNK;
    mem_slot = block_index & 0x1U;

    for (sample_index = 0U; sample_index < SENSOR_DMA_DB_CHUNK; sample_index++) {
        g_front_sensor_raw[dst_offset + sample_index] =
            g_front_sensor_dma_stage[mem_slot][sample_index];
        g_rear_sensor_raw[dst_offset + sample_index] =
            g_rear_sensor_dma_stage[mem_slot][sample_index];
    }

    g_front_last_committed_raw = g_front_sensor_raw[dst_offset + SENSOR_DMA_DB_CHUNK - 1U];
    g_rear_last_committed_raw = g_rear_sensor_raw[dst_offset + SENSOR_DMA_DB_CHUNK - 1U];
}

static void SensorCapture_CommitReadyBlocks(void) {
    uint32_t block_index;

    while ((g_consumed_full_pairs < g_front_full_count) &&
           (g_consumed_full_pairs < g_rear_full_count) &&
           (g_consumed_full_pairs < SENSOR_DMA_DB_BLOCKS)) {
        block_index = g_consumed_full_pairs;
        SensorCapture_CopyCommittedBlock(block_index);
        g_consumed_full_pairs++;
        g_dbg_cap_dma_pair_full_count++;
    }
}

static uint8_t SensorCapture_RegisterDmaComplete(uint8_t adc_id) {
    if (adc_id == 1U) {
        g_rear_full_count++;
        g_dbg_cap_dma_full_adc1_count++;
        if (g_rear_full_count > (g_front_full_count + 1U)) {
            g_dbg_cap_dma_pair_error_count++;
        }
        return 1U;
    }

    if (adc_id == 3U) {
        g_front_full_count++;
        g_dbg_cap_dma_full_adc3_count++;
        if (g_front_full_count > (g_rear_full_count + 1U)) {
            g_dbg_cap_dma_pair_error_count++;
        }
        return 1U;
    }

    g_dbg_cap_dma_spurious_cb_count++;
    return 0U;
}

static void SensorCapture_UpdateDmaDoneFlags(void) {
    if (g_front_full_count >= SENSOR_DMA_DB_BLOCKS) {
        g_front_dma_done = 1U;
    }

    if (g_rear_full_count >= SENSOR_DMA_DB_BLOCKS) {
        g_rear_dma_done = 1U;
    }
}

static void SensorCapture_FinishIfReady(void) {
    if ((g_front_dma_done == 0U) || (g_rear_dma_done == 0U)) {
        return;
    }

    g_state = SENSOR_CAP_DONE;
    g_processing_pending = 1U;
    g_dbg_cap_processing_pending_set_count++;
    g_dbg_cap_raw_done_us = App_TimeBase_GetUs();
    g_front_last_committed_raw = g_front_sensor_raw[SENSOR_SAMPLE_COUNT - 1U];
    g_rear_last_committed_raw = g_rear_sensor_raw[SENSOR_SAMPLE_COUNT - 1U];
    SensorCapture_StopDma();
    SensorCapture_UpdateDebug();
}

static void SensorCapture_StartDma(void) {
    HAL_StatusTypeDef st_front;
    HAL_StatusTypeDef st_rear;

    g_front_full_count = 0U;
    g_rear_full_count = 0U;
    g_consumed_full_pairs = 0U;
    g_front_dma_done = 0U;
    g_rear_dma_done = 0U;

    /* Els DMA alternen entre M0 i M1 mentre consolidem blocs de 100 mostres. */
    st_front = AppHwPatch_StartAdcDoubleBufferedDma(
        &hadc3,
        &g_front_sensor_dma_stage[0][0],
        &g_front_sensor_dma_stage[1][0],
        SENSOR_DMA_DB_CHUNK
    );
    st_rear = AppHwPatch_StartAdcDoubleBufferedDma(
        &hadc1,
        &g_rear_sensor_dma_stage[0][0],
        &g_rear_sensor_dma_stage[1][0],
        SENSOR_DMA_DB_CHUNK
    );

    if ((st_front != HAL_OK) || (st_rear != HAL_OK)) {
        if (st_front == HAL_OK) {
            (void)HAL_ADC_Stop_DMA(&hadc3);
        }

        if (st_rear == HAL_OK) {
            (void)HAL_ADC_Stop_DMA(&hadc1);
        }

        g_dma_running = 0U;
        g_state = SENSOR_CAP_ERROR;
        g_dbg_cap_dma_start_error_count++;
        SensorCapture_UpdateDebug();
        return;
    }

    g_dma_running = 1U;
}

static void SensorCapture_RequestSample(void) {
    if (g_dma_running == 0U) {
        return;
    }

    if (g_sample_index >= SENSOR_SAMPLE_COUNT) {
        return;
    }

    /* Una coincidencia de patro equival a una unica mostra espacial. */
    SensorCapture_RecordTriggerEvent();
    g_sample_index++;
    App_AdcTrigger_Schedule();
    SensorCapture_UpdateDebug();
}

static void SensorCapture_HandleEdge(uint8_t pin_id, uint16_t gpio_pin) {
    uint8_t current_level;

    if ((g_state != SENSOR_CAP_WAIT_FIRST_EDGE) && (g_state != SENSOR_CAP_ACQUIRING)) {
        return;
    }

    current_level = SensorCapture_GetPinLevel(gpio_pin);

    if (g_state == SENSOR_CAP_WAIT_FIRST_EDGE) {
        /* El primer flanc despres d'IDX fixa quin patro repetirem. */
        SensorCapture_LockFirstEdge(pin_id, current_level);
        SensorCapture_RequestSample();
        return;
    }

    if (SensorCapture_IsMatchingEdge(pin_id, current_level) != 0U) {
        g_dbg_cap_edge_match_count++;
        SensorCapture_RequestSample();
    } else {
        g_dbg_cap_edge_mismatch_count++;
    }

    SensorCapture_UpdateDebug();
}

void SensorCapture_Init(void) {
    SensorCapture_ClearRawBuffers();
    SensorCapture_ClearStageBuffers();
    SensorCapture_ResetRuntimeState();
    SensorCapture_ResetDebugCounters();

    SensorCapture_UpdateDebug();
}

void SensorCapture_OnIndex(void) {
    /* Cada IDX reinicia la captura d'una banda nova. */
    SensorCapture_StopDma();

    g_state = SENSOR_CAP_WAIT_FIRST_EDGE;
    g_sample_index = 0U;
    g_first_edge_pin = 0U;
    g_first_edge_level = 0U;
    g_processing_pending = 0U;
    SensorCapture_ResetDmaProgress();

    g_dbg_cap_index_events++;
    SensorCapture_ResetDebugAcquisitionCounters();

    SensorCapture_StartDma();
    SensorCapture_UpdateDebug();
}

void SensorCapture_OnEdgeA(void) {
    SensorCapture_HandleEdge(0U, INA_Pin);
}

void SensorCapture_OnEdgeB(void) {
    SensorCapture_HandleEdge(1U, INB_Pin);
}

void SensorCapture_OnAdcDmaHalfComplete(uint8_t adc_id) {
    (void)adc_id;

    /* En mode double buffer no usem interrupts de mig buffer. */
    g_dbg_cap_dma_spurious_cb_count++;
}

void SensorCapture_OnAdcDmaComplete(uint8_t adc_id) {
    if (g_dma_running == 0U) {
        g_dbg_cap_dma_spurious_cb_count++;
        return;
    }

    if (SensorCapture_RegisterDmaComplete(adc_id) == 0U) {
        return;
    }

    SensorCapture_CommitReadyBlocks();
    SensorCapture_UpdateDmaDoneFlags();
    SensorCapture_UpdateDebug();
    SensorCapture_FinishIfReady();
}

void SensorCapture_Task(void) {
    SensorCapture_UpdateDebug();
}

bool SensorCapture_IsAcquisitionDone(void) {
    return (g_state == SENSOR_CAP_DONE);
}

bool SensorCapture_IsProcessingPending(void) {
    return (g_processing_pending != 0U);
}

void SensorCapture_ClearProcessingPending(void) {
    g_processing_pending = 0U;
    g_dbg_cap_processing_pending_clear_count++;
    SensorCapture_UpdateDebug();
}

uint32_t SensorCapture_GetSampleCount(void) {
    return g_sample_index;
}

SensorCaptureState SensorCapture_GetState(void) {
    return g_state;
}
