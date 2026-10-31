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
#include "system_snapshot.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ntc_sensor.h"
#include "adc_dma.h"
#include "log.h"
#include "valves.h"
#include "water_level_steam.h"
#include "bldc_pump.h"
#include "steam_pwm_valve.h"
#include "water_level_tank.h"
#include "system_snapshot.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NTC_10K_INIT {.sh_a = 1.042752e-03f, .sh_b = 2.513051e-04f, .sh_c = -4.180126e-09f, .r_pull = 1500.0f}
// #define TEST_PUMP
// #define TEST_PWM_VALVE
#define TEST_TANK_PROBE
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
ntc_sensor_t ntc_preheat_sensor = NTC_10K_INIT;
ntc_sensor_t ntc_steam_sensor = NTC_10K_INIT;
ntc_sensor_t ntc_coffee_sensor = NTC_10K_INIT;

bldc_pump_t pump;
steam_pwm_valve_t steam_pwm_valve;
water_level_tank_t tank_level_probe;
water_probe_level_t steam_probe;
system_snapshot_t snapshot;
volatile uint8_t test_percent = 0;
volatile uint8_t test_reset = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_StatusTypeDef DMA_Status = adc_dma_init(&hadc1);
  if (DMA_Status != HAL_OK)
  {
    // Handle error
    Error_Handler();
  }

  valve_init();

  pump_init(&pump, &htim4, TIM_CHANNEL_1, pump_tacho_Pin);
  log_init(&huart1);
  init_ntc_sensor(&ntc_preheat_sensor, adc_dma_get_raw(ntc_preheat));
  init_ntc_sensor(&ntc_steam_sensor, adc_dma_get_raw(ntc_steam));
  init_ntc_sensor(&ntc_coffee_sensor, adc_dma_get_raw(ntc_coffee));
  init_water_level(&steam_probe, &htim1, TIM_CHANNEL_1);
  init_steam_pwm_valve(&steam_pwm_valve, &htim3, TIM_CHANNEL_1);
  init_water_level_tank(&tank_level_probe);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#ifdef TEST_PUMP
    pump_set(&pump, test_percent);
    update_pump_tacho(&pump);
    if (test_reset)
    {
      reset_fault_pump(&pump);
    }

    static uint32_t last_log_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_log_time >= 2000)
    {
      water_probe_update(&steam_probe, adc_dma_get_amplitude(water_level_steam));
      log_write("Probe: amp=%u, %s | Pump: set=%u%%, rpm=%u, %s\r\n",
                (unsigned)steam_probe.amplitude,
                water_probe_state_text(steam_probe.state),
                (unsigned)pump.percent,
                (unsigned)pump.rpm,
                pump_state_text(pump.state));
      last_log_time = current_time;
    }
#endif
#ifdef TEST_PWM_VALVE
    steam_pwm_valve_set(&steam_pwm_valve, test_percent);
    steam_pwm_valve_update(&steam_pwm_valve);
    if (test_reset)
    {
      steam_pwm_valve_reset_fault(&steam_pwm_valve);
    }

    static uint32_t last_log_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_log_time >= 2000)
    {
      water_probe_update(&steam_probe, adc_dma_get_amplitude(water_level_steam));
      log_write("Probe: amp=%u, %s | Valve: set=%u%%, %s\r\n",
                (unsigned)steam_probe.amplitude,
                water_probe_state_text(steam_probe.state),
                (unsigned)steam_pwm_valve.percent,
                pwm_valve_steam_state_text(steam_pwm_valve.state));
      last_log_time = current_time;
    }
#endif
#ifdef TEST_TANK_PROBE
    static uint32_t last_log_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_log_time >= 2000)
    {

      pump_set(&pump, test_percent);
      update_pump_tacho(&pump);
      get_temperature(&ntc_coffee_sensor, adc_dma_get_amplitude(ntc_coffee));
      get_temperature(&ntc_steam_sensor, adc_dma_get_amplitude (ntc_steam));
      get_temperature(&ntc_preheat_sensor, adc_dma_get_amplitude(ntc_preheat));
      water_probe_update(&steam_probe, adc_dma_get_amplitude(water_level_steam));
      water_tank_level_tank_update(&tank_level_probe, adc_dma_get_amplitude(water_level_tank_min), adc_dma_get_amplitude(water_level_tank_max));
      snapshot_update(&snapshot, &ntc_coffee_sensor, &ntc_steam_sensor, &ntc_preheat_sensor,
                      &pump, &steam_pwm_valve, &tank_level_probe, &steam_probe);
      log_write("t=%lu seq=%u faults=0x%02X valves=0x%02X pump=%u, rpm=%u\r\n",
                (unsigned long)snapshot.up_time_ms, (unsigned)snapshot.seq,
                (unsigned)snapshot.faults, (unsigned)snapshot.valves,
                (unsigned)snapshot.state_pump, (unsigned)snapshot.rpm_pump);
      last_log_time = current_time;
    }
#endif
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
