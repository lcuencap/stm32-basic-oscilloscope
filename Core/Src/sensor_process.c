#include "sensor_process.h"
#include "memory_map.h"
#include "timers.h"

#define SENSOR_PROC_STEP          (SENSOR_SAMPLE_COUNT / SENSOR_PROC_COUNT)
#define SENSOR_PROC_WINDOW_RADIUS 2U

float g_front_sensor_proc[SENSOR_PROC_COUNT];
float g_rear_sensor_proc[SENSOR_PROC_COUNT];

SensorStats g_front_stats;
SensorStats g_rear_stats;

volatile float g_dbg_front_avg_mm = 0.0f;
volatile float g_dbg_front_min_mm = 0.0f;
volatile float g_dbg_front_max_mm = 0.0f;
volatile float g_dbg_rear_avg_mm = 0.0f;
volatile float g_dbg_rear_min_mm = 0.0f;
volatile float g_dbg_rear_max_mm = 0.0f;
volatile uint32_t g_dbg_proc_run_count = 0U;
volatile uint32_t g_dbg_proc_last_start_us = 0U;
volatile uint32_t g_dbg_proc_last_end_us = 0U;
volatile uint32_t g_dbg_proc_last_dt_us = 0U;
volatile uint32_t g_dbg_proc_dt_min_us = 0xFFFFFFFFUL;
volatile uint32_t g_dbg_proc_dt_max_us = 0U;
volatile uint32_t g_dbg_proc_front_stats_ok = 0U;
volatile uint32_t g_dbg_proc_rear_stats_ok = 0U;
volatile uint32_t g_dbg_proc_front_oob_count = 0U;
volatile uint32_t g_dbg_proc_rear_oob_count = 0U;
volatile float g_dbg_proc_front_first_mm = 0.0f;
volatile float g_dbg_proc_front_last_mm = 0.0f;
volatile float g_dbg_proc_rear_first_mm = 0.0f;
volatile float g_dbg_proc_rear_last_mm = 0.0f;

static float SensorProcess_RawToVoltage(uint16_t raw) {
    return ((float)raw * 3.3f) / 4095.0f;
}

static float SensorProcess_VoltageToMm(float voltage) {
    float mm;

    mm = (voltage - 0.125f) * (2.0f / 2.25f);

    if (mm < 0.0f) {
        mm = 0.0f;
    }

    if (mm > 2.0f) {
        mm = 2.0f;
    }

    return mm;
}

static float SensorProcess_RawToMm(uint16_t raw) {
    return SensorProcess_VoltageToMm(SensorProcess_RawToVoltage(raw));
}

static float SensorProcess_AverageWindowMm(const uint16_t *raw, uint32_t center, uint32_t radius) {
    uint32_t first;
    uint32_t last;
    uint32_t i;
    uint32_t count;
    float sum;

    first = (center > radius) ? (center - radius) : 0U;
    last = center + radius;
    if (last >= SENSOR_SAMPLE_COUNT) {
        last = SENSOR_SAMPLE_COUNT - 1U;
    }

    sum = 0.0f;
    count = 0U;
    for (i = first; i <= last; i++) {
        sum += SensorProcess_RawToMm(raw[i]);
        count++;
    }

    return sum / (float)count;
}

static void SensorProcess_ComputeStats(const uint16_t *raw, SensorStats *stats, volatile uint32_t *oob_count) {
    uint32_t i;
    float value;
    float sum;
    float voltage;
    uint32_t local_oob_count;

    local_oob_count = 0U;

    value = SensorProcess_RawToMm(raw[0]);
    stats->min_mm = value;
    stats->max_mm = value;
    sum = 0.0f;

    for (i = 0U; i < SENSOR_SAMPLE_COUNT; i++) {
        voltage = SensorProcess_RawToVoltage(raw[i]);
        if ((voltage <= 0.125f) || (voltage >= 2.375f)) {
            local_oob_count++;
        }
        value = SensorProcess_VoltageToMm(voltage);
        sum += value;

        if (value < stats->min_mm) {
            stats->min_mm = value;
        }

        if (value > stats->max_mm) {
            stats->max_mm = value;
        }
    }

    stats->avg_mm = sum / (float)SENSOR_SAMPLE_COUNT;
    *oob_count = local_oob_count;
}

static void SensorProcess_ComputeProcTable(const uint16_t *raw, float *proc) {
    uint32_t k;
    uint32_t center;

    proc[0] = SensorProcess_AverageWindowMm(raw, 0U, SENSOR_PROC_WINDOW_RADIUS);

    for (k = 1U; k < (SENSOR_PROC_COUNT - 1U); k++) {
        center = k * SENSOR_PROC_STEP;
        proc[k] = SensorProcess_AverageWindowMm(raw, center, SENSOR_PROC_WINDOW_RADIUS);
    }

    proc[SENSOR_PROC_COUNT - 1U] =
        SensorProcess_AverageWindowMm(raw, SENSOR_SAMPLE_COUNT - 1U, SENSOR_PROC_WINDOW_RADIUS);
}

void SensorProcess_Run(void) {
    uint32_t start_us;
    uint32_t end_us;
    uint32_t dt_us;

    start_us = App_TimeBase_GetUs();
    g_dbg_proc_last_start_us = start_us;
    g_dbg_proc_run_count++;

    SensorProcess_ComputeStats(g_front_sensor_raw, &g_front_stats, &g_dbg_proc_front_oob_count);
    SensorProcess_ComputeStats(g_rear_sensor_raw, &g_rear_stats, &g_dbg_proc_rear_oob_count);

    SensorProcess_ComputeProcTable(g_front_sensor_raw, g_front_sensor_proc);
    SensorProcess_ComputeProcTable(g_rear_sensor_raw, g_rear_sensor_proc);

    g_dbg_front_avg_mm = g_front_stats.avg_mm;
    g_dbg_front_min_mm = g_front_stats.min_mm;
    g_dbg_front_max_mm = g_front_stats.max_mm;
    g_dbg_rear_avg_mm = g_rear_stats.avg_mm;
    g_dbg_rear_min_mm = g_rear_stats.min_mm;
    g_dbg_rear_max_mm = g_rear_stats.max_mm;
    g_dbg_proc_front_stats_ok = ((g_front_stats.min_mm <= g_front_stats.avg_mm) &&
                                 (g_front_stats.avg_mm <= g_front_stats.max_mm)) ? 1U : 0U;
    g_dbg_proc_rear_stats_ok = ((g_rear_stats.min_mm <= g_rear_stats.avg_mm) &&
                                (g_rear_stats.avg_mm <= g_rear_stats.max_mm)) ? 1U : 0U;
    g_dbg_proc_front_first_mm = g_front_sensor_proc[0];
    g_dbg_proc_front_last_mm = g_front_sensor_proc[SENSOR_PROC_COUNT - 1U];
    g_dbg_proc_rear_first_mm = g_rear_sensor_proc[0];
    g_dbg_proc_rear_last_mm = g_rear_sensor_proc[SENSOR_PROC_COUNT - 1U];

    end_us = App_TimeBase_GetUs();
    dt_us = end_us - start_us;
    g_dbg_proc_last_end_us = end_us;
    g_dbg_proc_last_dt_us = dt_us;
    if (dt_us < g_dbg_proc_dt_min_us) {
        g_dbg_proc_dt_min_us = dt_us;
    }
    if (dt_us > g_dbg_proc_dt_max_us) {
        g_dbg_proc_dt_max_us = dt_us;
    }
}
