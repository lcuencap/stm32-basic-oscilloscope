#ifndef ENCODER_SIMULATOR_H
#define ENCODER_SIMULATOR_H

#include <stdint.h>

extern volatile uint8_t g_sim_force_reverse;
extern volatile uint32_t g_sim_start_state;
extern volatile uint32_t g_sim_reverse_at_edge;
extern volatile uint8_t g_sim_auto_reverse_enable;

void EncoderSim_Init(void);
void EncoderSim_StartBand(void);
void EncoderSim_Stop(void);
void EncoderSim_SetSpeed_mm_s(uint32_t speed_mm_s);
void EncoderSim_Step(void);
void EncoderSim_Task(void);
uint8_t EncoderSim_IsRunning(void);

#endif
