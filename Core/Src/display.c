/*
 * display.c - 5 rutines de dibuix de la P3 + integracio amb els sensors.
 *
 * Pixel ARGB4444 (16 bits): A[15:12] R[11:8] G[7:4] B[3:0].
 * Framebuffer per layer = 240 (amplada fisica) x 320 (alcada fisica) pixels.
 * Coordenades logiques (col, row) = 320 x 240 apaisat -> rotacio 90 a la
 * fisica: x_phys = row, y_phys = 319 - col.
 */

#include "display.h"
#include "memory_map.h"
#include "ltdc.h"
#include "timers.h"

#define LCD_W_LOG       320U   /* Amplada logica (apaisat). */
#define LCD_H_LOG       240U   /* Alcada logica  (apaisat). */
#define LCD_W_PHYS      240U   /* Amplada fisica (retrat). */
#define LCD_H_PHYS      320U   /* Alcada  fisica (retrat). */

/* Adreces dels framebuffers (ARGB4444, 2 bytes/pixel). */
#define LCD_LAYER0_BASE  ((uint32_t)0xD0000000)
#define LCD_LAYER1_BASE  ((uint32_t)0xD0050000)
#define LCD_FB_BYTES     (LCD_W_PHYS * LCD_H_PHYS * 2U)

/* Finestres de visualitzacio. Marc exterior 302x103, area util 300x101. */
#define WIN_FRONT_X0      8U
#define WIN_FRONT_Y0      8U
#define WIN_FRONT_X1    309U   /* 8 + 302 - 1 */
#define WIN_FRONT_Y1    110U   /* 8 + 103 - 1 */
#define WIN_FRONT_DATA_X0  (WIN_FRONT_X0 + 1U)   /* 9   */
#define WIN_FRONT_DATA_X1  (WIN_FRONT_X1 - 1U)   /* 308 */
#define WIN_FRONT_DATA_Y0  (WIN_FRONT_Y0 + 1U)   /* 9   */
#define WIN_FRONT_DATA_Y1  (WIN_FRONT_Y1 - 1U)   /* 109 */

#define WIN_REAR_X0       8U
#define WIN_REAR_Y0     118U
#define WIN_REAR_X1     309U
#define WIN_REAR_Y1     220U
#define WIN_REAR_DATA_X0  (WIN_REAR_X0 + 1U)    /* 9   */
#define WIN_REAR_DATA_X1  (WIN_REAR_X1 - 1U)    /* 308 */
#define WIN_REAR_DATA_Y0  (WIN_REAR_Y0 + 1U)    /* 119 */
#define WIN_REAR_DATA_Y1  (WIN_REAR_Y1 - 1U)    /* 219 */

/* Rectangles identificadors de color. */
#define ID_FRONT_X0      50U
#define ID_FRONT_X1      90U
#define ID_FRONT_Y0     228U
#define ID_FRONT_Y1     235U
#define ID_REAR_X0      230U
#define ID_REAR_X1      270U
#define ID_REAR_Y0      228U
#define ID_REAR_Y1      235U

/* Colors ARGB4444 (4 bits per canal). */
#define COL_BLACK_A   0xFU
#define COL_BLACK_R   0x0U
#define COL_BLACK_G   0x0U
#define COL_BLACK_B   0x0U

#define COL_WHITE_A   0xFU
#define COL_WHITE_R   0xFU
#define COL_WHITE_G   0xFU
#define COL_WHITE_B   0xFU

#define COL_FRONT_A   0xFU   /* verd opac per al sensor davanter */
#define COL_FRONT_R   0x0U
#define COL_FRONT_G   0xFU
#define COL_FRONT_B   0x0U

#define COL_REAR_A    0xFU   /* vermell opac per al sensor posterior */
#define COL_REAR_R    0xFU
#define COL_REAR_G    0x0U
#define COL_REAR_B    0x0U

#define COL_TRANSP_A  0x0U   /* alpha=0 -> pixel transparent (deixa veure layer 0) */
#define COL_TRANSP_R  0x0U
#define COL_TRANSP_G  0x0U
#define COL_TRANSP_B  0x0U

/* Layer activa de les rutines de dibuix. Per defecte, Layer 1 (formes d'ona). */
static uint32_t g_active_layer_base = LCD_LAYER1_BASE;

/* Variables de debug. */
volatile uint32_t g_dbg_display_init_count          = 0U;
volatile uint32_t g_dbg_display_refresh_count       = 0U;
volatile uint32_t g_dbg_display_setpixel_oob_count  = 0U;
volatile uint32_t g_dbg_display_last_refresh_dt_us  = 0U;
volatile uint32_t g_dbg_display_last_refresh_start_us = 0U;
volatile uint32_t g_dbg_display_last_refresh_end_us   = 0U;

/* Empaqueta un color ARGB4444 en un uint16_t. */
static inline uint16_t Display_PackArgb4444(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((uint16_t)(a & 0x0FU) << 12) |
                      ((uint16_t)(r & 0x0FU) << 8)  |
                      ((uint16_t)(g & 0x0FU) << 4)  |
                      ((uint16_t)(b & 0x0FU)));
}

/* Calcula l'adreca del pixel (col, row) dins de la layer indicada,
 * aplicant la rotacio 90 (apaisat -> retrat fisic). */
static inline volatile uint16_t *Display_PixelAddr(uint32_t layer_base,
                                                   uint16_t col, uint16_t row) {
    uint32_t x_phys = (uint32_t)row;                       /* 0..239 */
    uint32_t y_phys = (uint32_t)(LCD_W_LOG - 1U - col);    /* 319..0 */
    uint32_t offset = (y_phys * LCD_W_PHYS + x_phys) * 2U;
    return (volatile uint16_t *)(layer_base + offset);
}

/* Selecciona quina layer escriuen les rutines (us intern de Display_Init). */
static void Display_SetActiveLayer(uint32_t layer_base) {
    g_active_layer_base = layer_base;
}

/* Reaplica la configuracio de les dues layers amb ImageWidth/Height correctes.
 * CubeMX ha deixat aquests camps a 0 i sense aixo el LTDC no llegeix res. */
static void Display_FixLtdcLayers(void) {
    LTDC_LayerCfgTypeDef cfg = {0};

    cfg.WindowX0       = 0;
    cfg.WindowX1       = LCD_W_PHYS;
    cfg.WindowY0       = 0;
    cfg.WindowY1       = LCD_H_PHYS;
    cfg.PixelFormat    = LTDC_PIXEL_FORMAT_ARGB4444;
    cfg.Alpha          = 255;
    cfg.Alpha0         = 0;
    cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    cfg.ImageWidth     = LCD_W_PHYS;
    cfg.ImageHeight    = LCD_H_PHYS;

    cfg.FBStartAdress  = LCD_LAYER0_BASE;
    (void)HAL_LTDC_ConfigLayer(&hltdc, &cfg, 0);

    cfg.FBStartAdress  = LCD_LAYER1_BASE;
    (void)HAL_LTDC_ConfigLayer(&hltdc, &cfg, 1);
}

/* Dibuixa el marc rectangular d'una finestra (4 linies). */
static void Display_DrawFrame(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    (void)DibuixaLiniaHoritzontal(x0, x1, y0, COL_BLACK_A, COL_BLACK_R, COL_BLACK_G, COL_BLACK_B);
    (void)DibuixaLiniaHoritzontal(x0, x1, y1, COL_BLACK_A, COL_BLACK_R, COL_BLACK_G, COL_BLACK_B);
    (void)DibuixaLiniaVertical  (x0, y0, y1, COL_BLACK_A, COL_BLACK_R, COL_BLACK_G, COL_BLACK_B);
    (void)DibuixaLiniaVertical  (x1, y0, y1, COL_BLACK_A, COL_BLACK_R, COL_BLACK_G, COL_BLACK_B);
}

/* Omple un rectangle solid amb el color indicat (linia a linia via SetPixel). */
static void Display_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                             uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t row = y0; row <= y1; row++) {
        (void)DibuixaLiniaHoritzontal(x0, x1, row, a, r, g, b);
    }
}

/* Converteix un valor en mm [0..2] a una fila dins de la finestra.
 * 0 mm -> fila inferior, 2 mm -> fila superior, 0.02 mm/pixel. */
static uint16_t Display_MmToRowInWindow(float mm, uint16_t bottom_row) {
    int offset = (int)(mm * 50.0f + 0.5f);   /* arrodonit; 0..100 */
    if (offset < 0)    offset = 0;
    if (offset > 100)  offset = 100;
    return (uint16_t)((int)bottom_row - offset);
}

/* Pinta els 300 punts d'un sensor dins de la seva finestra. */
static void Display_PlotSensor(const float *proc, uint16_t data_x0,
                               uint16_t data_y_bottom,
                               uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    for (uint32_t i = 0U; i < SENSOR_PROC_COUNT; i++) {
        uint16_t col = (uint16_t)(data_x0 + i);
        uint16_t row = Display_MmToRowInWindow(proc[i], data_y_bottom);
        (void)SetPixel(col, row, a, r, g, b);
    }
}

RetSt SetPixel(uint16_t col, uint16_t row,
               uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval) {
    if ((col >= LCD_W_LOG) || (row >= LCD_H_LOG)) {
        g_dbg_display_setpixel_oob_count++;
        return NO_OK;
    }
    volatile uint16_t *p = Display_PixelAddr(g_active_layer_base, col, row);
    *p = Display_PackArgb4444(alpha, Rval, Gval, Bval);
    return OK;
}

RetSt DibuixaLiniaHoritzontal(uint16_t col_inici, uint16_t col_fi, uint16_t row,
                              uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval) {
    if (row >= LCD_H_LOG)                            return NO_OK;
    if ((col_inici >= LCD_W_LOG) || (col_fi >= LCD_W_LOG)) return NO_OK;

    uint16_t a = (col_inici <= col_fi) ? col_inici : col_fi;
    uint16_t b = (col_inici <= col_fi) ? col_fi    : col_inici;
    for (uint16_t c = a; c <= b; c++) {
        (void)SetPixel(c, row, alpha, Rval, Gval, Bval);
    }
    return OK;
}

RetSt DibuixaLiniaVertical(uint16_t col, uint16_t row_inici, uint16_t row_fi,
                           uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval) {
    if (col >= LCD_W_LOG)                              return NO_OK;
    if ((row_inici >= LCD_H_LOG) || (row_fi >= LCD_H_LOG)) return NO_OK;

    uint16_t a = (row_inici <= row_fi) ? row_inici : row_fi;
    uint16_t b = (row_inici <= row_fi) ? row_fi    : row_inici;
    for (uint16_t r = a; r <= b; r++) {
        (void)SetPixel(col, r, alpha, Rval, Gval, Bval);
    }
    return OK;
}

/* Esborra el contingut de la layer activa amb el color indicat.
 * Implementacio estricta a base de SetPixel, tal com demana l'enunciat. */
RetSt EsborraDades(uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval) {
    for (uint16_t row = 0U; row < LCD_H_LOG; row++) {
        for (uint16_t col = 0U; col < LCD_W_LOG; col++) {
            (void)SetPixel(col, row, alpha, Rval, Gval, Bval);
        }
    }
    return OK;
}

/* Esborra tota la pantalla (les dues layers) amb el color indicat.
 * Tambe a base de SetPixel: nomes canviem la layer activa entre passades. */
RetSt EsborraPantalla(uint8_t alpha, uint8_t Rval, uint8_t Gval, uint8_t Bval) {
    uint32_t prev_layer = g_active_layer_base;

    Display_SetActiveLayer(LCD_LAYER0_BASE);
    for (uint16_t row = 0U; row < LCD_H_LOG; row++) {
        for (uint16_t col = 0U; col < LCD_W_LOG; col++) {
            (void)SetPixel(col, row, alpha, Rval, Gval, Bval);
        }
    }

    Display_SetActiveLayer(LCD_LAYER1_BASE);
    for (uint16_t row = 0U; row < LCD_H_LOG; row++) {
        for (uint16_t col = 0U; col < LCD_W_LOG; col++) {
            (void)SetPixel(col, row, alpha, Rval, Gval, Bval);
        }
    }

    Display_SetActiveLayer(prev_layer);
    return OK;
}

void Display_Init(void) {
    g_dbg_display_init_count++;

    /* 1) Arregla ImageWidth/Height de les layers (CubeMX les deixa a 0). */
    Display_FixLtdcLayers();

    /* 2) Pinta el Layer 0 amb fons blanc opac via EsborraDades. */
    Display_SetActiveLayer(LCD_LAYER0_BASE);
    (void)EsborraDades(COL_WHITE_A, COL_WHITE_R, COL_WHITE_G, COL_WHITE_B);

    /* 3) Dibuixa els elements estatics (marcs i identificadors de color) al Layer 0. */
    Display_DrawFrame(WIN_FRONT_X0, WIN_FRONT_Y0, WIN_FRONT_X1, WIN_FRONT_Y1);
    Display_DrawFrame(WIN_REAR_X0,  WIN_REAR_Y0,  WIN_REAR_X1,  WIN_REAR_Y1);

    Display_FillRect(ID_FRONT_X0, ID_FRONT_Y0, ID_FRONT_X1, ID_FRONT_Y1,
                     COL_FRONT_A, COL_FRONT_R, COL_FRONT_G, COL_FRONT_B);
    Display_FillRect(ID_REAR_X0,  ID_REAR_Y0,  ID_REAR_X1,  ID_REAR_Y1,
                     COL_REAR_A,  COL_REAR_R,  COL_REAR_G,  COL_REAR_B);

    /* 4) Layer 1 totalment transparent (deixa veure els marcs del Layer 0). */
    Display_SetActiveLayer(LCD_LAYER1_BASE);
    (void)EsborraDades(COL_TRANSP_A, COL_TRANSP_R, COL_TRANSP_G, COL_TRANSP_B);
}

void Display_RefreshFromSensors(void) {
    g_dbg_display_refresh_count++;
    g_dbg_display_last_refresh_start_us = App_TimeBase_GetUs();

    /* Treballem sempre sobre el Layer 1 (dinamic). */
    Display_SetActiveLayer(LCD_LAYER1_BASE);

    /* Neteja l'area dinamica deixant-la transparent. */
    (void)EsborraDades(COL_TRANSP_A, COL_TRANSP_R, COL_TRANSP_G, COL_TRANSP_B);

    /* Sensor davanter (verd). */
    Display_PlotSensor(g_front_sensor_proc,
                       WIN_FRONT_DATA_X0, WIN_FRONT_DATA_Y1,
                       COL_FRONT_A, COL_FRONT_R, COL_FRONT_G, COL_FRONT_B);

    /* Sensor posterior (vermell). */
    Display_PlotSensor(g_rear_sensor_proc,
                       WIN_REAR_DATA_X0, WIN_REAR_DATA_Y1,
                       COL_REAR_A, COL_REAR_R, COL_REAR_G, COL_REAR_B);

    g_dbg_display_last_refresh_end_us = App_TimeBase_GetUs();
    g_dbg_display_last_refresh_dt_us  =
        g_dbg_display_last_refresh_end_us - g_dbg_display_last_refresh_start_us;
}
