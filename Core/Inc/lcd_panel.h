#ifndef INC_LCD_PANEL_H_
#define INC_LCD_PANEL_H_

/*
 * lcd_panel.h - Inicialitzacio del panell LCD (controlador ILI9341).
 *
 * Cadena: STM32F429 --LTDC--> ILI9341 --> TFT 240x320
 *                  \-SPI5-> ILI9341 (nomes durant l'Init).
 *
 * Pins: PC2 LCD_NCS, PD13 LCD_WRX, PF7/PF9 SPI5_SCK/MOSI.
 */

#include <stdint.h>

/* Executa la sequencia de power-on del ILI9341 (calcada de stm32f429i_discovery_lcd.c). */
void LcdPanel_Init(void);

/* Variables g_dbg_* per inspeccio des del debugger. */
extern volatile uint32_t g_dbg_lcd_panel_init_count;
extern volatile uint32_t g_dbg_lcd_panel_cmd_count;
extern volatile uint32_t g_dbg_lcd_panel_data_count;
extern volatile uint32_t g_dbg_lcd_panel_spi_errors;
extern volatile uint32_t g_dbg_lcd_panel_init_start_ms;
extern volatile uint32_t g_dbg_lcd_panel_init_end_ms;
extern volatile uint32_t g_dbg_lcd_panel_init_dt_ms;
extern volatile uint32_t g_dbg_lcd_panel_init_done;

#endif /* INC_LCD_PANEL_H_ */
