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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define RC5_HALF_MIN_US        640U
#define RC5_HALF_MAX_US        1140U
#define RC5_FULL_MIN_US        1340U
#define RC5_FULL_MAX_US        2220U
#define RC5_FRAME_BITS         14U
#define RC5_FRAME_HALFBITS     (RC5_FRAME_BITS * 2U)
#define RC5_BUFFER_HALFBITS    64U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

typedef struct
{
  uint16_t Raw;
  uint8_t StartBit;
  uint8_t FieldBit;
  uint8_t ToggleBit;
  uint8_t Address;
  uint8_t Command;
} IR_Frame_t;

volatile uint8_t RC5FrameReceived = RESET;
IR_Frame_t IR_FRAME;

static uint8_t rc5Collecting = 0U;
static uint8_t rc5HalfBits[RC5_BUFFER_HALFBITS];
static uint8_t rc5HalfCount = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

static void RC5_ResetState(void);
static uint8_t RC5_QuantizeHalfBits(uint32_t durationUs);
static void RC5_AppendLevel(uint8_t level, uint8_t count);
static void RC5_TryDecodeBuffer(void);
static void RC5_ProcessCapture(uint32_t periodUs, uint32_t lowUs);

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
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  setvbuf(stdout, NULL, _IONBF, 0);

  RC5_ResetState();

  if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  printf("RC5 receiver ready\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (RC5FrameReceived != RESET)
    {
      printf("[RC5] Raw: 0x%04X | Addr: 0x%02X | Cmd: 0x%02X | Toggle: %d\r\n",
             IR_FRAME.Raw,
             IR_FRAME.Address,
             IR_FRAME.Command,
             IR_FRAME.ToggleBit);
      RC5FrameReceived = RESET;
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 15;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3699;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
  sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
  sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_FALLING;
  sSlaveConfig.TriggerFilter = 3;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 3;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1U, 100U);
  return ch;
}

int _write(int file, char *ptr, int len)
{
  (void)file;
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if ((htim->Instance == TIM2) && (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1))
  {
    uint32_t periodUs = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    uint32_t lowUs = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    RC5_ProcessCapture(periodUs, lowUs);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    if (RC5FrameReceived == RESET)
    {
      RC5_ResetState();
    }
  }
}

static void RC5_ResetState(void)
{
  rc5Collecting = 0U;
  rc5HalfCount = 0U;
}

static uint8_t RC5_QuantizeHalfBits(uint32_t durationUs)
{
  if ((durationUs >= RC5_HALF_MIN_US) && (durationUs <= RC5_HALF_MAX_US))
  {
    return 1U;
  }
  if ((durationUs >= RC5_FULL_MIN_US) && (durationUs <= RC5_FULL_MAX_US))
  {
    return 2U;
  }
  return 0U;
}

static void RC5_AppendLevel(uint8_t level, uint8_t count)
{
  while ((count > 0U) && (rc5HalfCount < RC5_BUFFER_HALFBITS))
  {
    rc5HalfBits[rc5HalfCount++] = level;
    count--;
  }
}

static void RC5_TryDecodeBuffer(void)
{
  uint8_t offset;

  for (offset = 0U; (offset < 3U) && ((offset + RC5_FRAME_HALFBITS) <= rc5HalfCount); offset++)
  {
    uint8_t i;
    uint16_t raw = 0U;
    uint8_t valid = 1U;

    for (i = 0U; i < RC5_FRAME_BITS; i++)
    {
      uint8_t firstHalf = rc5HalfBits[offset + (2U * i)];
      uint8_t secondHalf = rc5HalfBits[offset + (2U * i) + 1U];

      if ((firstHalf == 1U) && (secondHalf == 0U))
      {
        raw = (uint16_t)((raw << 1) | 1U);
      }
      else if ((firstHalf == 0U) && (secondHalf == 1U))
      {
        raw = (uint16_t)(raw << 1);
      }
      else
      {
        valid = 0U;
        break;
      }
    }

    if ((valid != 0U) && (((raw >> 13) & 0x01U) == 1U) && (((raw >> 12) & 0x01U) == 1U))
    {
      uint8_t fieldBit = (uint8_t)((raw >> 12) & 0x01U);
      uint8_t command = (uint8_t)(raw & 0x3FU);

      if (fieldBit == 0U)
      {
        command |= 0x40U;
      }

      IR_FRAME.Raw = raw;
      IR_FRAME.StartBit = (uint8_t)((raw >> 13) & 0x01U);
      IR_FRAME.FieldBit = fieldBit;
      IR_FRAME.ToggleBit = (uint8_t)((raw >> 11) & 0x01U);
      IR_FRAME.Address = (uint8_t)((raw >> 6) & 0x1FU);
      IR_FRAME.Command = command;
      RC5FrameReceived = SET;

      RC5_ResetState();
      return;
    }
  }

  if (rc5HalfCount > 40U)
  {
    uint8_t keep = 28U;
    uint8_t i;
    for (i = 0U; i < keep; i++)
    {
      rc5HalfBits[i] = rc5HalfBits[rc5HalfCount - keep + i];
    }
    rc5HalfCount = keep;
  }
}

static void RC5_ProcessCapture(uint32_t periodUs, uint32_t lowUs)
{
  uint8_t lowHalfCount;
  uint8_t highHalfCount;
  uint32_t highUs;

  if ((periodUs == 0U) || (lowUs == 0U) || (lowUs >= periodUs))
  {
    RC5_ResetState();
    return;
  }

  if (rc5Collecting == 0U)
  {
    rc5Collecting = 1U;
    rc5HalfCount = 0U;
    RC5_AppendLevel(1U, 1U);
  }

  highUs = periodUs - lowUs;
  lowHalfCount = RC5_QuantizeHalfBits(lowUs);
  highHalfCount = RC5_QuantizeHalfBits(highUs);

  if ((lowHalfCount == 0U) || (highHalfCount == 0U))
  {
    RC5_ResetState();
    return;
  }

  RC5_AppendLevel(0U, lowHalfCount);
  RC5_AppendLevel(1U, highHalfCount);

  if (rc5HalfCount >= RC5_FRAME_HALFBITS)
  {
    RC5_TryDecodeBuffer();
  }
}

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
