#ifndef INC_MEMORY_MAP_H_
#define INC_MEMORY_MAP_H_

#include <stdint.h>

#define SENSOR_SAMPLE_COUNT   3000U
#define SENSOR_PROC_COUNT      300U

/* Captura crua dels ADC: 3000 mostres espacials per sensor. */
extern uint16_t g_front_sensor_raw[SENSOR_SAMPLE_COUNT];
extern uint16_t g_rear_sensor_raw[SENSOR_SAMPLE_COUNT];

/* Perfil processat: 300 punts resumits per sensor. */
extern float g_front_sensor_proc[SENSOR_PROC_COUNT];
extern float g_rear_sensor_proc[SENSOR_PROC_COUNT];

#define FrontSensor_ORG      ((uintptr_t)&g_front_sensor_raw[0])
#define RearSensor_ORG       ((uintptr_t)&g_rear_sensor_raw[0])
#define FrontSensorProc_ORG  ((uintptr_t)&g_front_sensor_proc[0])
#define RearSensorProc_ORG   ((uintptr_t)&g_rear_sensor_proc[0])

#endif
