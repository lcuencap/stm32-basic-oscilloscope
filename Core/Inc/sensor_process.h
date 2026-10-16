#ifndef INC_SENSOR_PROCESS_H_
#define INC_SENSOR_PROCESS_H_

#include <stdint.h>

typedef struct
{
    float avg_mm;
    float min_mm;
    float max_mm;
} SensorStats;

void SensorProcess_Run(void);

extern SensorStats g_front_stats;
extern SensorStats g_rear_stats;

extern volatile float g_dbg_front_avg_mm;
extern volatile float g_dbg_front_min_mm;
extern volatile float g_dbg_front_max_mm;
extern volatile float g_dbg_rear_avg_mm;
extern volatile float g_dbg_rear_min_mm;
extern volatile float g_dbg_rear_max_mm;
extern volatile uint32_t g_dbg_proc_run_count;
extern volatile uint32_t g_dbg_proc_last_start_us;
extern volatile uint32_t g_dbg_proc_last_end_us;
extern volatile uint32_t g_dbg_proc_last_dt_us;
extern volatile uint32_t g_dbg_proc_dt_min_us;
extern volatile uint32_t g_dbg_proc_dt_max_us;
extern volatile uint32_t g_dbg_proc_front_stats_ok;
extern volatile uint32_t g_dbg_proc_rear_stats_ok;
extern volatile uint32_t g_dbg_proc_front_oob_count;
extern volatile uint32_t g_dbg_proc_rear_oob_count;
extern volatile float g_dbg_proc_front_first_mm;
extern volatile float g_dbg_proc_front_last_mm;
extern volatile float g_dbg_proc_rear_first_mm;
extern volatile float g_dbg_proc_rear_last_mm;

#endif
