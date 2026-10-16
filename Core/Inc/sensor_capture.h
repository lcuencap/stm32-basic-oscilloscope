#ifndef INC_SENSOR_CAPTURE_H_
#define INC_SENSOR_CAPTURE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SENSOR_CAP_IDLE = 0,
    SENSOR_CAP_WAIT_FIRST_EDGE, /* Ja hem vist IDX i falta fixar el patro de flanc. */
    SENSOR_CAP_ACQUIRING,       /* Un dispar de mostra per cada flanc A/B que coincideix. */
    SENSOR_CAP_DONE,
    SENSOR_CAP_ERROR
} SensorCaptureState;

void SensorCapture_Init(void);
void SensorCapture_OnIndex(void);
void SensorCapture_OnEdgeA(void);
void SensorCapture_OnEdgeB(void);
void SensorCapture_OnAdcDmaHalfComplete(uint8_t adc_id);
void SensorCapture_OnAdcDmaComplete(uint8_t adc_id);
void SensorCapture_Task(void);

bool SensorCapture_IsAcquisitionDone(void);
bool SensorCapture_IsProcessingPending(void);
void SensorCapture_ClearProcessingPending(void);
uint32_t SensorCapture_GetSampleCount(void);
SensorCaptureState SensorCapture_GetState(void);

extern volatile uint32_t g_dbg_sensor_cap_state;
extern volatile uint32_t g_dbg_sensor_sample_count;
extern volatile uint32_t g_dbg_sensor_processing_pending;
extern volatile uint32_t g_dbg_front_last_raw;
extern volatile uint32_t g_dbg_rear_last_raw;
extern volatile uint32_t g_dbg_cap_index_events;
extern volatile uint32_t g_dbg_cap_first_lock_events;
extern volatile uint32_t g_dbg_cap_locked_pin;
extern volatile uint32_t g_dbg_cap_locked_level;
extern volatile uint32_t g_dbg_cap_edge_match_count;
extern volatile uint32_t g_dbg_cap_edge_mismatch_count;
extern volatile uint32_t g_dbg_cap_trigger_count;
extern volatile uint32_t g_dbg_cap_last_trigger_us;
extern volatile uint32_t g_dbg_cap_prev_trigger_us;
extern volatile uint32_t g_dbg_cap_trigger_dt_us;
extern volatile uint32_t g_dbg_cap_trigger_dt_min_us;
extern volatile uint32_t g_dbg_cap_trigger_dt_max_us;
extern volatile uint32_t g_dbg_cap_dma_half_adc1_count;
extern volatile uint32_t g_dbg_cap_dma_half_adc3_count;
extern volatile uint32_t g_dbg_cap_dma_full_adc1_count;
extern volatile uint32_t g_dbg_cap_dma_full_adc3_count;
extern volatile uint32_t g_dbg_cap_dma_pair_half_count;
extern volatile uint32_t g_dbg_cap_dma_pair_full_count;
extern volatile uint32_t g_dbg_cap_dma_pair_error_count;
extern volatile uint32_t g_dbg_cap_dma_spurious_cb_count;
extern volatile uint32_t g_dbg_cap_dma_start_error_count;
extern volatile uint32_t g_dbg_cap_raw_done_us;
extern volatile uint32_t g_dbg_cap_processing_pending_set_count;
extern volatile uint32_t g_dbg_cap_processing_pending_clear_count;

#endif
