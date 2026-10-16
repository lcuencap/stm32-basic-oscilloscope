/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "ltdc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "encoder.h"
#include "encoder_simulator.h"
#include "timers.h"
#include "sensor_capture.h"
#include "sensor_process.h"
#include "memory_map.h"
#include "lcd_panel.h"
#include "display.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SPEED_COUNT 5U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
static const uint32_t kSpeedTable_mm_s[SPEED_COUNT] = {100U, 125U, 150U, 175U, 200U};

volatile uint32_t g_sim_speed_index = 0U;
volatile uint32_t g_sim_speed_mm_s = 100U;
volatile uint32_t g_dbg_selected_speed_mm_s = 100U;
volatile uint32_t g_start_band_req = 0U;
volatile uint32_t g_dbg_main_proc_trigger_seen = 0U;
volatile uint32_t g_dbg_main_proc_trigger_cleared = 0U;
volatile uint32_t g_dbg_main_proc_runs = 0U;
volatile uint32_t g_dbg_main_last_proc_start_us = 0U;
volatile uint32_t g_dbg_main_last_proc_end_us = 0U;
volatile uint32_t g_dbg_main_last_proc_dt_us = 0U;
volatile uint32_t g_dbg_main_proc_overrun_count = 0U;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef enum {
    APP_IDLE = 0,
    APP_RUNNING,
    APP_PROCESSING
} AppState;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint32_t App_GetSelectedSpeed_mm_s(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t App_GetSelectedSpeed_mm_s(void) {
    if (g_sim_speed_index >= SPEED_COUNT) {
        g_sim_speed_index = 0U;
    }
    return kSpeedTable_mm_s[g_sim_speed_index];
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    /* Cada interrupcio GPIO alimenta el modul d'encoder i el de captura. */
    if (GPIO_Pin == INIDX_Pin) {
        Encoder_OnIndex();
        SensorCapture_OnIndex();
        return;
    }

    if (GPIO_Pin == INA_Pin) {
        Encoder_OnEdgeA();
        SensorCapture_OnEdgeA();
        return;
    }

    if (GPIO_Pin == INB_Pin) {
        Encoder_OnEdgeB();
        SensorCapture_OnEdgeB();
        return;
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    /* ADC1 = sensor posterior, ADC3 = sensor davanter. */
    if (hadc->Instance == ADC1) {
        SensorCapture_OnAdcDmaComplete(1U);
        return;
    }

    if (hadc->Instance == ADC3) {
        SensorCapture_OnAdcDmaComplete(3U);
        return;
    }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        SensorCapture_OnAdcDmaHalfComplete(1U);
        return;
    }

    if (hadc->Instance == ADC3) {
        SensorCapture_OnAdcDmaHalfComplete(3U);
        return;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_ADC3_Init();
  MX_FMC_Init();
  MX_LTDC_Init();
  MX_SPI5_Init();
  /* USER CODE BEGIN 2 */
  App_Timers_Init();
  App_TimeBase_Start();

  Encoder_Init();
  EncoderSim_Init();
  SensorCapture_Init();

  /* Encen el panell LCD (ILI9341) i pinta els elements estatics. */
  LcdPanel_Init();
  Display_Init();

  g_sim_speed_mm_s = App_GetSelectedSpeed_mm_s();
  g_dbg_selected_speed_mm_s = g_sim_speed_mm_s;
  EncoderSim_SetSpeed_mm_s(g_sim_speed_mm_s);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  {
    uint32_t last_speed_mm_s = g_sim_speed_mm_s;
    AppState app_state = APP_IDLE;
    const EncoderStatus *enc;
    uint8_t start_req_armed = 1U;
    static uint8_t process_busy = 0U;

    while (1)
    {
      uint32_t proc_start_us;
      uint32_t proc_end_us;

      g_sim_speed_mm_s = App_GetSelectedSpeed_mm_s();
      g_dbg_selected_speed_mm_s = g_sim_speed_mm_s;

      if (g_sim_speed_mm_s != last_speed_mm_s) {
          EncoderSim_SetSpeed_mm_s(g_sim_speed_mm_s);
          last_speed_mm_s = g_sim_speed_mm_s;
      }

      EncoderSim_Task();
      SensorCapture_Task();
      enc = Encoder_GetStatus();

      switch (app_state) {
          case APP_IDLE:
              if (g_start_band_req == 0U) {
                  start_req_armed = 1U;
              } else if (start_req_armed != 0U) {
                  g_start_band_req = 0U;
                  start_req_armed = 0U;
                  Encoder_Init();
                  EncoderSim_SetSpeed_mm_s(g_sim_speed_mm_s);
                  EncoderSim_StartBand();
                  app_state = APP_RUNNING;
              } else {
                  g_start_band_req = 0U;
              }
              break;

          case APP_RUNNING:
              g_start_band_req = 0U;
              start_req_armed = 0U;
              if ((enc->state == ENC_DONE) || (enc->state == ENC_ERROR)) {
                  EncoderSim_Stop();
                  app_state = (enc->state == ENC_DONE) ? APP_PROCESSING : APP_IDLE;
              }
              break;

          case APP_PROCESSING:
              g_start_band_req = 0U;
              start_req_armed = 0U;
              if (SensorCapture_IsProcessingPending()) {
                  g_dbg_main_proc_trigger_seen++;
                  if (process_busy != 0U) {
                      g_dbg_main_proc_overrun_count++;
                  }
                  process_busy = 1U;
                  SensorCapture_ClearProcessingPending();
                  g_dbg_main_proc_trigger_cleared++;
                  proc_start_us = App_TimeBase_GetUs();
                  g_dbg_main_last_proc_start_us = proc_start_us;
                  SensorProcess_Run();
                  /* Actualitza les ones a la LCD amb les dades acabades de calcular. */
                  Display_RefreshFromSensors();
                  proc_end_us = App_TimeBase_GetUs();
                  g_dbg_main_last_proc_end_us = proc_end_us;
                  g_dbg_main_last_proc_dt_us = proc_end_us - proc_start_us;
                  g_dbg_main_proc_runs++;
                  process_busy = 0U;
                  app_state = APP_IDLE;
              } else if (SensorCapture_GetState() == SENSOR_CAP_ERROR) {
                  app_state = APP_IDLE;
              }
              break;

          default:
              EncoderSim_Stop();
              g_start_band_req = 0U;
              start_req_armed = 0U;
              app_state = APP_IDLE;
              break;
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
