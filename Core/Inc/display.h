#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

/*
 * display.h - Rutines de dibuix de la P3 i integracio amb els sensors.
 *
 * Coordenades logiques: 320 (col) x 240 (row), origen a dalt-esquerra.
 * Format de pixel: ARGB4444 (16 bits). Framebuffers en SDRAM:
 *   Layer 0 -> 0xD0000000 (marcs + rectangles, estatic)
 *   Layer 1 -> 0xD0050000 (formes d'ona, dinamic)
 */

#include <stdint.h>

/* Struct de retorn (el que exigeix l'enunciat). */
typedef enum {
    NO_OK = 0,
    OK    = !NO_OK
} RetSt;

/* Les 5 rutines obligatories de l'enunciat. */
RetSt SetPixel(uint16_t col, uint16_t row,
               uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval);

RetSt DibuixaLiniaHoritzontal(uint16_t col_inici, uint16_t col_fi, uint16_t row,
                              uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval);

RetSt DibuixaLiniaVertical(uint16_t col, uint16_t row_inici, uint16_t row_fi,
                           uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval);

RetSt EsborraDades(uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval);

RetSt EsborraPantalla(uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval);

/* Pinta marcs i rectangles identificadors (una vegada, durant l'arrencada). */
void Display_Init(void);

/* Refresca les dues formes d'ona amb el contingut actual de
 * g_front_sensor_proc[] i g_rear_sensor_proc[]. Cridar quan acaba la banda. */
void Display_RefreshFromSensors(void);

/* Variables g_dbg_* per inspeccio des del debugger. */
extern volatile uint32_t g_dbg_display_init_count;
extern volatile uint32_t g_dbg_display_refresh_count;
extern volatile uint32_t g_dbg_display_setpixel_oob_count;
extern volatile uint32_t g_dbg_display_last_refresh_dt_us;
extern volatile uint32_t g_dbg_display_last_refresh_start_us;
extern volatile uint32_t g_dbg_display_last_refresh_end_us;

#endif /* INC_DISPLAY_H_ */
