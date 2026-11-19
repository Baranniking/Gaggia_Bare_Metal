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
#include "boiler_ctrl.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* drivers */
#include "adc_dma.h"
#include "bldc_pump.h"
#include "heater.h"
#include "log.h"
#include "ntc_sensor.h"
#include "steam_pwm_valve.h"
#include "valves.h"
#include "water_level_steam.h"
#include "water_level_tank.h"
/* app */
#include "system_snapshot.h"
/* link */
#include "cmd_handler.h"
#include "esp_link.h"
#include "tlm_adapter.h"
#include "pid.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NTC_10K_INIT {.sh_a = 1.042752e-03f, .sh_b = 2.513051e-04f, .sh_c = -4.180126e-09f, .r_pull = 1500.0f}
#define SENSORS_PERIOD_MS 50u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

ntc_sensor_t ntc_preheat_sensor = NTC_10K_INIT;
ntc_sensor_t ntc_steam_sensor = NTC_10K_INIT;
ntc_sensor_t ntc_coffee_sensor = NTC_10K_INIT;

static bldc_pump_t pump;
static steam_pwm_valve_t steam_pwm_valve;
static water_level_tank_t tank_level_probe;
static water_probe_level_t steam_probe;

static pid_ctrl_t preheat_pid;
static pid_ctrl_t steam_pid;
static pid_ctrl_t coffee_pid;

static boiler_t preheat_boiler;
static boiler_t steam_boiler;
static boiler_t coffee_boiler;

static system_snapshot_t snapshot;
static msg_telemetry_t tlm;
static proto_frame_t rx_frame;

/* ESP link TX statistics (for debugging) */
static uint32_t count_ok, count_busy, count_error, count_timeout;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void app_init(void);
static void link_service(void);
static void sensors_update(void);
static void telemetry_send(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. *c  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  app_init();
      /* USER CODE END 2 */

      /* Infinite loop */
      /* USER CODE BEGIN WHILE */
      while (1)
  {
    link_service();

    heater_update_all();
    update_pump_tacho(&pump);
    steam_pwm_valve_update(&steam_pwm_valve);

    static uint32_t last_time = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_time >= SENSORS_PERIOD_MS)
    {
      last_time = now;
      sensors_update();
      telemetry_send();
    }

    static uint32_t last_time_update_boiler = 0;
    if (now - last_time_update_boiler >= 500)
    {
      boiler_ctrl_update(&preheat_boiler);
      boiler_ctrl_update(&steam_boiler);
      boiler_ctrl_update(&coffee_boiler);
      last_time_update_boiler = now;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/** Initialize drivers and the ESP link. */
static void app_init(void)
{
  reset_cause_save();

  if (adc_dma_init(&hadc1) != HAL_OK)
    Error_Handler();
  if (init_esp_link(&huart6) != HAL_OK)
    Error_Handler();

  valve_init();
  heaters_init();
  pump_init(&pump, &htim4, TIM_CHANNEL_1, pump_tacho_Pin);
  log_init(&huart1);
  init_ntc_sensor(&ntc_preheat_sensor, adc_dma_get_raw(ntc_preheat));
  init_ntc_sensor(&ntc_steam_sensor, adc_dma_get_raw(ntc_steam));
  init_ntc_sensor(&ntc_coffee_sensor, adc_dma_get_raw(ntc_coffee));
  init_water_level(&steam_probe, &htim1, TIM_CHANNEL_1);
  init_steam_pwm_valve(&steam_pwm_valve, &htim3, TIM_CHANNEL_1);
  init_water_level_tank(&tank_level_probe);
  if (init_pid(&preheat_pid, 10.0f, 3.0f, 0, 0.5f, 1000, 0) != HAL_OK || init_pid(&steam_pid, 10.0f, 3.0f, 0, 0.5f, 1000, 0) != HAL_OK || init_pid(&coffee_pid, 10.0f, 3.0f, 0, 0.5f, 1000, 0) != HAL_OK || init_boiler(&preheat_boiler, &ntc_preheat_sensor, &preheat_pid, PRE_HEATER, 95.0f, 70.0f) != HAL_OK || init_boiler(&steam_boiler, &ntc_steam_sensor, &steam_pid, STEAM_HEATER, 130.0f, 100.0f) != HAL_OK || init_boiler(&coffee_boiler, &ntc_coffee_sensor, &coffee_pid, COFFEE_HEATER, 100.0f, 50.0f) != HAL_OK)
  {
    heater_stop_all();
    while (1)
    {
      /* code */
    }
    

  }
  if(init_cmd_handler(&preheat_boiler, &steam_boiler, &coffee_boiler) != HAL_OK)
  {
    heater_stop_all();
    while(1)
    {

    }
  }
}

/** Receive and handle a command frame from the display, if any. */
static void link_service(void)
{
  if (esp_link_tx_ready() && esp_link_poll(&rx_frame))
  {
    cmd_send_hello();
    cmd_handler(&rx_frame);
  }
}

/** Read all sensors and refresh the system snapshot. */
static void sensors_update(void)
{
  get_temperature(&ntc_coffee_sensor, adc_dma_get_raw(ntc_coffee));
  get_temperature(&ntc_steam_sensor, adc_dma_get_raw(ntc_steam));
  get_temperature(&ntc_preheat_sensor, adc_dma_get_raw(ntc_preheat));
  water_probe_update(&steam_probe, adc_dma_get_amplitude(water_level_steam));
  water_tank_level_tank_update(&tank_level_probe,
                               adc_dma_get_amplitude(water_level_tank_min),
                               adc_dma_get_amplitude(water_level_tank_max));
  snapshot_update(&snapshot, &ntc_coffee_sensor, &ntc_steam_sensor, &ntc_preheat_sensor,
                  &pump, &steam_pwm_valve, &tank_level_probe, &steam_probe);
}

/** Convert the snapshot to a telemetry message and send it to the display. */
static void telemetry_send(void)
{
  tlm_from_snapshot(&snapshot, &tlm);

  switch (esp_link_send(MSG_TELEMETRY, &tlm, sizeof(tlm)))
  {
  case HAL_OK:
    count_ok++;
    break;
  case HAL_BUSY:
    count_busy++;
    break;
  case HAL_ERROR:
    count_error++;
    break;
  case HAL_TIMEOUT:
    count_timeout++;
    break;
  default:
    break;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  pump.impulse_tacho++;
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  valve_closed_all();
  pump_stop(&pump);
  steam_pwm_valve_stop(&steam_pwm_valve);
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
