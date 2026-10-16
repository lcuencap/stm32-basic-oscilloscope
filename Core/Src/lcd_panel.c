/*
 * lcd_panel.c - Init del panell LCD basat en el controlador ILI9341 (ILITEK).
 *
 * El ILI9341 es l'IC darrere del TFT. Es configura per SPI5 (init) i despres
 * rep els pixels directament del LTDC pel bus paral.lel RGB.
 *
 * Aquest modul nomes fa l'Init. El dibuix a memoria es a display.c.
 */

#include "lcd_panel.h"
#include "main.h"
#include "spi.h"

/* Registres del ILI9341 (datasheet "Command List"). */
#define ILI9341_SLEEP_OUT       0x11
#define ILI9341_GAMMA           0x26
#define ILI9341_DISPLAY_ON      0x29
#define ILI9341_COLUMN_ADDR     0x2A   /* Rang horitzontal de la GRAM. */
#define ILI9341_PAGE_ADDR       0x2B   /* Rang vertical de la GRAM. */
#define ILI9341_GRAM            0x2C   /* Inici escriptura GRAM. */
#define ILI9341_MAC             0x36   /* Orientacio (Memory Access Control). */
#define ILI9341_RGB_INTERFACE   0xB0   /* Activa interficie RGB (LTDC). */
#define ILI9341_FRC             0xB1   /* Frame Rate Control. */
#define ILI9341_DFC             0xB6   /* Display Function Control. */
#define ILI9341_POWER1          0xC0
#define ILI9341_POWER2          0xC1
#define ILI9341_VCOM1           0xC5
#define ILI9341_VCOM2           0xC7
#define ILI9341_POWERA          0xCB
#define ILI9341_POWERB          0xCF
#define ILI9341_PGAMMA          0xE0   /* Positive Gamma. */
#define ILI9341_NGAMMA          0xE1   /* Negative Gamma. */
#define ILI9341_DTCA            0xE8
#define ILI9341_DTCB            0xEA
#define ILI9341_POWER_SEQ       0xED
#define ILI9341_3GAMMA_EN       0xF2
#define ILI9341_INTERFACE       0xF6
#define ILI9341_PRC             0xF7

#define LCD_PANEL_SPI_TIMEOUT_MS  100U

/* Variables de debug. */
volatile uint32_t g_dbg_lcd_panel_init_count    = 0U;
volatile uint32_t g_dbg_lcd_panel_cmd_count     = 0U;
volatile uint32_t g_dbg_lcd_panel_data_count    = 0U;
volatile uint32_t g_dbg_lcd_panel_spi_errors    = 0U;
volatile uint32_t g_dbg_lcd_panel_init_start_ms = 0U;
volatile uint32_t g_dbg_lcd_panel_init_end_ms   = 0U;
volatile uint32_t g_dbg_lcd_panel_init_dt_ms    = 0U;
volatile uint32_t g_dbg_lcd_panel_init_done     = 0U;

/* Envia 1 byte de comanda (WRX=0). */
static void LcdPanel_WriteCmd(uint8_t cmd) {
    HAL_StatusTypeDef st;

    HAL_GPIO_WritePin(LCD_WRX_GPIO_Port, LCD_WRX_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_RESET);

    st = HAL_SPI_Transmit(&hspi5, &cmd, 1U, LCD_PANEL_SPI_TIMEOUT_MS);
    if (st != HAL_OK) { g_dbg_lcd_panel_spi_errors++; }

    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_SET);
    g_dbg_lcd_panel_cmd_count++;
}

/* Envia 1 byte de dada (WRX=1). Sempre despres d'un WriteCmd. */
static void LcdPanel_WriteData(uint8_t data) {
    HAL_StatusTypeDef st;

    HAL_GPIO_WritePin(LCD_WRX_GPIO_Port, LCD_WRX_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_RESET);

    st = HAL_SPI_Transmit(&hspi5, &data, 1U, LCD_PANEL_SPI_TIMEOUT_MS);
    if (st != HAL_OK) { g_dbg_lcd_panel_spi_errors++; }

    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_SET);
    g_dbg_lcd_panel_data_count++;
}

/* Power-on del ILI9341. Sequencia identica a LCD_PowerOn() de la llibreria oficial. */
void LcdPanel_Init(void) {
    g_dbg_lcd_panel_init_count++;
    g_dbg_lcd_panel_init_start_ms = HAL_GetTick();

    /* Estat inicial dels pins de control. */
    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_WRX_GPIO_Port, LCD_WRX_Pin, GPIO_PIN_RESET);

    /* Power control inicial. */
    LcdPanel_WriteCmd(0xCA);
    LcdPanel_WriteData(0xC3); LcdPanel_WriteData(0x08); LcdPanel_WriteData(0x50);

    LcdPanel_WriteCmd(ILI9341_POWERB);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0xC1); LcdPanel_WriteData(0x30);

    LcdPanel_WriteCmd(ILI9341_POWER_SEQ);
    LcdPanel_WriteData(0x64); LcdPanel_WriteData(0x03);
    LcdPanel_WriteData(0x12); LcdPanel_WriteData(0x81);

    LcdPanel_WriteCmd(ILI9341_DTCA);
    LcdPanel_WriteData(0x85); LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x78);

    LcdPanel_WriteCmd(ILI9341_POWERA);
    LcdPanel_WriteData(0x39); LcdPanel_WriteData(0x2C); LcdPanel_WriteData(0x00);
    LcdPanel_WriteData(0x34); LcdPanel_WriteData(0x02);

    LcdPanel_WriteCmd(ILI9341_PRC);
    LcdPanel_WriteData(0x20);

    LcdPanel_WriteCmd(ILI9341_DTCB);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x00);

    /* Frame rate ~70 Hz. */
    LcdPanel_WriteCmd(ILI9341_FRC);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x1B);

    /* Display function (primer ajust). */
    LcdPanel_WriteCmd(ILI9341_DFC);
    LcdPanel_WriteData(0x0A); LcdPanel_WriteData(0xA2);

    /* Tensions del panell. */
    LcdPanel_WriteCmd(ILI9341_POWER1); LcdPanel_WriteData(0x10);
    LcdPanel_WriteCmd(ILI9341_POWER2); LcdPanel_WriteData(0x10);

    LcdPanel_WriteCmd(ILI9341_VCOM1);
    LcdPanel_WriteData(0x45); LcdPanel_WriteData(0x15);

    LcdPanel_WriteCmd(ILI9341_VCOM2); LcdPanel_WriteData(0x90);

    /* Orientacio retrat amb mirall. La rotacio a 320x240 logica la fa display.c. */
    LcdPanel_WriteCmd(ILI9341_MAC); LcdPanel_WriteData(0xC8);

    /* Desactiva correccio de 3 gamma. */
    LcdPanel_WriteCmd(ILI9341_3GAMMA_EN); LcdPanel_WriteData(0x00);

    /* Activa interficie RGB (mode LTDC). */
    LcdPanel_WriteCmd(ILI9341_RGB_INTERFACE); LcdPanel_WriteData(0xC2);

    /* Display function (segon ajust, 4 bytes). */
    LcdPanel_WriteCmd(ILI9341_DFC);
    LcdPanel_WriteData(0x0A); LcdPanel_WriteData(0xA7);
    LcdPanel_WriteData(0x27); LcdPanel_WriteData(0x04);

    /* Finestra activa de la GRAM: tota la pantalla (0..239 x 0..319). */
    LcdPanel_WriteCmd(ILI9341_COLUMN_ADDR);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x00);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0xEF);   /* 239 */

    LcdPanel_WriteCmd(ILI9341_PAGE_ADDR);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x00);
    LcdPanel_WriteData(0x01); LcdPanel_WriteData(0x3F);   /* 319 */

    /* Interface control. */
    LcdPanel_WriteCmd(ILI9341_INTERFACE);
    LcdPanel_WriteData(0x01); LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x06);

    /* Tanca la programacio inicial de la GRAM i espera estabilitzacio. */
    LcdPanel_WriteCmd(ILI9341_GRAM);
    HAL_Delay(200U);

    /* Corbes de gamma. */
    LcdPanel_WriteCmd(ILI9341_GAMMA); LcdPanel_WriteData(0x01);

    LcdPanel_WriteCmd(ILI9341_PGAMMA);
    LcdPanel_WriteData(0x0F); LcdPanel_WriteData(0x29); LcdPanel_WriteData(0x24);
    LcdPanel_WriteData(0x0C); LcdPanel_WriteData(0x0E); LcdPanel_WriteData(0x09);
    LcdPanel_WriteData(0x4E); LcdPanel_WriteData(0x78); LcdPanel_WriteData(0x3C);
    LcdPanel_WriteData(0x09); LcdPanel_WriteData(0x13); LcdPanel_WriteData(0x05);
    LcdPanel_WriteData(0x17); LcdPanel_WriteData(0x11); LcdPanel_WriteData(0x00);

    LcdPanel_WriteCmd(ILI9341_NGAMMA);
    LcdPanel_WriteData(0x00); LcdPanel_WriteData(0x16); LcdPanel_WriteData(0x1B);
    LcdPanel_WriteData(0x04); LcdPanel_WriteData(0x11); LcdPanel_WriteData(0x07);
    LcdPanel_WriteData(0x31); LcdPanel_WriteData(0x33); LcdPanel_WriteData(0x42);
    LcdPanel_WriteData(0x05); LcdPanel_WriteData(0x0C); LcdPanel_WriteData(0x0A);
    LcdPanel_WriteData(0x28); LcdPanel_WriteData(0x2F); LcdPanel_WriteData(0x0F);

    /* Sleep out, espera wake-up (200 ms del datasheet) i encen la sortida. */
    LcdPanel_WriteCmd(ILI9341_SLEEP_OUT);
    HAL_Delay(200U);
    LcdPanel_WriteCmd(ILI9341_DISPLAY_ON);

    /* GRAM lista per rebre dades del LTDC. */
    LcdPanel_WriteCmd(ILI9341_GRAM);

    g_dbg_lcd_panel_init_end_ms = HAL_GetTick();
    g_dbg_lcd_panel_init_dt_ms  = g_dbg_lcd_panel_init_end_ms - g_dbg_lcd_panel_init_start_ms;
    g_dbg_lcd_panel_init_done   = 1U;
}
