#ifndef GPIO_MAP_H
#define GPIO_MAP_H

#include "main.h"

/* Entrades de l'encoder. */
#define INA_PORT        INA_GPIO_Port
#define INA_PIN_MASK    INA_Pin

#define INB_PORT        INB_GPIO_Port
#define INB_PIN_MASK    INB_Pin

#define INIDX_PORT      INIDX_GPIO_Port
#define INIDX_PIN_MASK  INIDX_Pin

/* Sortides principals de la practica. */
#define OUTPULSE_PORT   OUTPULSE_GPIO_Port
#define OUTPULSE_PIN_MASK OUTPULSE_Pin

#define OUTEOP_PORT     OUTEOP_GPIO_Port
#define OUTEOP_PIN_MASK OUTEOP_Pin

#define OUTERR_PORT     OUTERR_GPIO_Port
#define OUTERR_PIN_MASK OUTERR_Pin

/* Sortides del simulador intern. */
#define OUTSIMA_PORT    OUTSIMA_GPIO_Port
#define OUTSIMA_PIN_MASK OUTSIMA_Pin

#define OUTSIMB_PORT    OUTSIMB_GPIO_Port
#define OUTSIMB_PIN_MASK OUTSIMB_Pin

#define OUTSIMIDX_PORT  OUTSIMIDX_GPIO_Port
#define OUTSIMIDX_PIN_MASK OUTSIMIDX_Pin

/* Pols de debug per veure l'activitat de la ISR. */
#define DBG_ISR_PORT    DBG_ISR_GPIO_Port
#define DBG_ISR_PIN_MASK DBG_ISR_Pin

#endif
