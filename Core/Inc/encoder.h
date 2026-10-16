#ifndef INC_ENCODER_H_
#define INC_ENCODER_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    DIR_UNKNOWN = 0,
    DIR_RTL, /* Direccio valida: de dreta a esquerra. */
    DIR_LTR
} EncDirection;

typedef enum
{
    ENC_IDLE = 0,
    ENC_PRINTING,
    ENC_DONE,
    ENC_ERROR
} EncState;

typedef struct
{
    volatile EncState state;
    volatile uint32_t edge_count;
    volatile uint32_t t_start_us;
    volatile uint32_t t_end_us;
    volatile uint32_t avg_speed_mm_s;
    volatile EncDirection last_dir;
    volatile bool idx_seen;
    volatile bool eop;
    volatile bool error;
} EncoderStatus;

void Encoder_Init(void);
void Encoder_OnIndex(void);
void Encoder_OnEdgeA(void);
void Encoder_OnEdgeB(void);

const EncoderStatus *Encoder_GetStatus(void);

#endif /* INC_ENCODER_H_ */
