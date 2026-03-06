/**
  ******************************************************************************
  * @file    rc5_encode.c
  * @author  MCD Application Team
  * @brief   This file provides all the rc5 encode firmware functions
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2018 STMicroelectronics. 
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the 
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ir_common.h"
#include "rc5_encode.h"

/* Private_Defines -----------------------------------------------------------*/
#define  RC5HIGHSTATE     ((uint8_t )0x02)   /* RC5 high level definition*/
#define  RC5LOWSTATE      ((uint8_t )0x01)   /* RC5 low level definition*/

/* Private_Function_Prototypes -----------------------------------------------*/
static uint16_t RC5_BinFrameGeneration(uint8_t RC5_Address, uint8_t RC5_Instruction, RC5_Ctrl_t RC5_Ctrl);
static uint32_t RC5_ManchesterConvert(uint16_t RC5_BinaryFrameFormat);
static void RC5_Encode_DeInit(void);

/* Private_Variables ---------------------------------------------------------*/
TIM_HandleTypeDef TimHandleLF;  /* TIM15: Low Frequency (bit timing) */
TIM_HandleTypeDef TimHandleHF;  /* TIM16: High Frequency (38kHz carrier) */
uint8_t RC5RealFrameLength = 14;
uint8_t RC5GlobalFrameLength = 64;
uint16_t RC5BinaryFrameFormat = 0;
uint32_t RC5ManchesterFrameFormat = 0;
__IO uint32_t RC5SendOpCompleteFlag = 1;
__IO uint32_t RC5SendOpReadyFlag = 0;
RC5_Ctrl_t RC5Ctrl1 = RC5_CTRL_RESET;

/* Exported variables from ir_common.h */
uint8_t BitsSentCounter = 0;
uint8_t AddressIndex = 0;
uint8_t InstructionIndex = 0;
__IO StatusOperation_t RFDemoStatus = NONE;

/* Exported_Functions--------------------------------------------------------*/

/**
  * @brief  RC5 Encoder initialization for STM32L432KC Nucleo
  * @note   Simplified version without LCD/Menu for Nucleo board
  * @param  None
  * @retval None
  */
void Menu_RC5_Encode_Func(void)
{
  /* Initialize RC5 encoder hardware (timers, GPIO) */
  RC5_Encode_Init();
  
  /* Set default values */
  AddressIndex = 0;
  RFDemoStatus = RC5_ENC;
  InstructionIndex = 0;
  
  /* RC5 encoder is now ready to use
   * Call RC5_Encode_SendFrame(address, instruction, ctrl) to send IR commands
   * Example: RC5_Encode_SendFrame(0, 12, RC5_CTRL_RESET);
   */
}

/**
  * @brief  De-initializes the peripherals (GPIO, TIM) for STM32L432KC
  * @param  None
  * @retval None
  */
static void RC5_Encode_DeInit(void)
{
  HAL_TIM_OC_DeInit(&TimHandleLF);
  HAL_TIM_OC_DeInit(&TimHandleHF);
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_6);  /* PA6 for TIM16 - IR_LED */
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2);  /* PA2 for TIM15 - data envelope */
}

/**
  * @brief Init Hardware (IPs used) for RC5 generation on STM32L432KC
  * @note  Uses TIM16 for 38kHz carrier and TIM15 for bit timing
  * @param None
  * @retval  None
  */
void RC5_Encode_Init(void)
{
  TIM_OC_InitTypeDef ch_config;
  GPIO_InitTypeDef gpio_init_struct;

  /* Enable timer clocks */
  __HAL_RCC_TIM16_CLK_ENABLE();      /* TIM16 clock enable for carrier */
  __HAL_RCC_TIM15_CLK_ENABLE();      /* TIM15 clock enable for bit timing */
  __HAL_RCC_GPIOA_CLK_ENABLE();      /* GPIOA clock enable */

  TimHandleLF.Instance = TIM15;      /* TIM15 for bit timing */
  TimHandleHF.Instance = TIM16;      /* TIM16 for 38kHz carrier */

  /* Configure GPIO pin : PA6 for TIM16_CH1 (38kHz carrier output - IR_LED) */
  gpio_init_struct.Pin = GPIO_PIN_6;        /* PA6 */
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init_struct.Alternate = GPIO_AF14_TIM16;  /* AF14 for TIM16 */
  HAL_GPIO_Init(GPIOA, &gpio_init_struct);

  /* Configure GPIO pin : PA2 for TIM15_CH1 (LF envelope - data modulation) */
  gpio_init_struct.Pin = GPIO_PIN_2;        /* PA2 */
  gpio_init_struct.Mode = GPIO_MODE_AF_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init_struct.Alternate = GPIO_AF14_TIM15;  /* AF14 for TIM15 */
  HAL_GPIO_Init(GPIOA, &gpio_init_struct);

  /* ===== TIM16 Configuration: 38kHz Carrier Generator ===== */
  HAL_TIM_PWM_DeInit(&TimHandleHF);
  
  /* Configure TIM16 for 38kHz PWM carrier
   * System Clock = 32MHz (MSI 4MHz * PLL)
   * Timer clock = 32MHz / (PSC+1) = 32MHz / 1 = 32MHz
   * PWM frequency = 32MHz / (ARR+1) = 32MHz / 843 = 37.97 kHz
   * Duty cycle = CCR / ARR = 210 / 842 = 24.94%
   */
  TimHandleHF.Init.Period = 842;               /* 842 for 38kHz at 32MHz */
  TimHandleHF.Init.Prescaler = 0;              /* PSC = 0 (no prescaling) */
  TimHandleHF.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  TimHandleHF.Init.CounterMode = TIM_COUNTERMODE_UP;
  TimHandleHF.Init.RepetitionCounter = 0;
  TimHandleHF.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  
  if (HAL_TIM_PWM_Init(&TimHandleHF) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* PWM Mode configuration: Channel 1 */
  ch_config.OCMode = TIM_OCMODE_PWM1;
  ch_config.Pulse = 210;                       /* 210 for ~25% duty */
  ch_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  ch_config.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  ch_config.OCFastMode = TIM_OCFAST_DISABLE;
  ch_config.OCIdleState = TIM_OCIDLESTATE_RESET;
  ch_config.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  
  if (HAL_TIM_PWM_ConfigChannel(&TimHandleHF, &ch_config, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Configuration Error */
    Error_Handler();
  }

  /* Start PWM on TIM16 CH1 */
  HAL_TIM_PWM_Start(&TimHandleHF, TIM_CHANNEL_1);
  
  /* Enable Main Output for TIM16 (required for advanced timers!) */
  __HAL_TIM_MOE_ENABLE(&TimHandleHF);

  /* ===== TIM15 Configuration: RC5 Bit Timing (889us period) ===== */
  HAL_TIM_OC_DeInit(&TimHandleLF);

  /* Configure TIM15 for RC5 bit timing
   * System Clock = 32MHz
   * Timer clock = 32MHz / (PSC+1) = 32MHz / 1 = 32MHz
   * Bit period = 889us -> 32MHz * 0.000889s = 28448 ticks - 1 = 28447
   */
  TimHandleLF.Init.Prescaler = 0;              /* PSC = 0 -> 32MHz */
  TimHandleLF.Init.CounterMode = TIM_COUNTERMODE_UP;
  TimHandleLF.Init.Period = 28447;             /* 28447 ticks for 889us at 32MHz */
  TimHandleLF.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  TimHandleLF.Init.RepetitionCounter = 0;
  TimHandleLF.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  
  if (HAL_TIM_Base_Init(&TimHandleLF) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }
  
  if (HAL_TIM_PWM_Init(&TimHandleLF) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /* Configure TIM15 Channel 1 in PWM mode for RC5 bit generation */
  ch_config.OCMode = TIM_OCMODE_PWM1;
  ch_config.Pulse = 0;                         /* Start with 0, will be modulated */
  ch_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  ch_config.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  ch_config.OCFastMode = TIM_OCFAST_DISABLE;
  ch_config.OCIdleState = TIM_OCIDLESTATE_RESET;
  ch_config.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  
  if (HAL_TIM_PWM_ConfigChannel(&TimHandleLF, &ch_config, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Configuration Error */
    Error_Handler();
  }

  /* Configure TIM15 interrupt */
  HAL_NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);

  /* Start TIM15 PWM output on Channel 1 (PA2) */
  HAL_TIM_PWM_Start(&TimHandleLF, TIM_CHANNEL_1);

  /* TIM15 is kept disabled until RC5_Encode_SendFrame is called */
  __HAL_TIM_DISABLE(&TimHandleLF);
}

/**
  * @brief Generate and Send the RC5 frame.
  * @param RC5_Address : the RC5 Device destination
  * @param RC5_Instruction : the RC5 command instruction
  * @param RC5_Ctrl : the RC5 Control bit.
  * @retval  None
  */
void RC5_Encode_SendFrame(uint8_t RC5_Address, uint8_t RC5_Instruction, RC5_Ctrl_t RC5_Ctrl)
{
  /* Generate a binary format of the Frame */
  RC5BinaryFrameFormat = RC5_BinFrameGeneration(RC5_Address, RC5_Instruction, RC5_Ctrl);

  /* Generate a Manchester format of the Frame */
  RC5ManchesterFrameFormat = RC5_ManchesterConvert(RC5BinaryFrameFormat);

  /* Set the Send operation Ready flag to indicate that the frame is ready to be sent */
  RC5SendOpReadyFlag = 1;

  /* Reset the counter to ensure accurate timing of sync pulse */
  __HAL_TIM_SET_COUNTER( &TimHandleLF, 0);

  /* TIM IT Enable */
  HAL_TIM_Base_Start_IT(&TimHandleLF);
}

/**
  * @brief Send by hardware Manchester Format RC5 Frame.
  * @retval None
  */
void RC5_Encode_SignalGenerate(void)
{
  uint32_t bit_msg = 0;

  if ((RC5SendOpReadyFlag == 1) && (BitsSentCounter <= (RC5GlobalFrameLength * 2)))
  {
    RC5SendOpCompleteFlag = 0x00;
    bit_msg = (uint8_t)((RC5ManchesterFrameFormat >> BitsSentCounter) & 1);

    if (bit_msg == 1)
    {
      /* Manchester '1': carrier active */
      TIM_ForcedOC1Config(TIM_FORCED_ACTIVE);
      /* Set TIM15 PWM duty cycle to 100% (full period) for data visualization on PA2 */
      __HAL_TIM_SET_COMPARE(&TimHandleLF, TIM_CHANNEL_1, TimHandleLF.Init.Period);
    }
    else
    {
      /* Manchester '0': carrier inactive */
      TIM_ForcedOC1Config(TIM_FORCED_INACTIVE);
      /* Set TIM15 PWM duty cycle to 0% for data visualization on PA2 */
      __HAL_TIM_SET_COMPARE(&TimHandleLF, TIM_CHANNEL_1, 0);
    }
    BitsSentCounter++;
  }
  else
  {
    RC5SendOpCompleteFlag = 0x01;

    /* TIM IT Disable */
    HAL_TIM_Base_Stop_IT(&TimHandleLF);
    RC5SendOpReadyFlag = 0;
    BitsSentCounter = 0;
    TIM_ForcedOC1Config(TIM_FORCED_INACTIVE);

    /* Set TIM15 PWM to 0% */
    __HAL_TIM_SET_COMPARE(&TimHandleLF, TIM_CHANNEL_1, 0);

    /* TIM Disable */
    __HAL_TIM_DISABLE(&TimHandleLF);
  }
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief Generate the binary format of the RC5 frame.
  * @param RC5_Address : Select the device address.
  * @param RC5_Instruction : Select the device instruction.
  * @param RC5_Ctrl : Select the device control bit status.
  * @retval Binary format of the RC5 Frame.
  */
static uint16_t RC5_BinFrameGeneration(uint8_t RC5_Address, uint8_t RC5_Instruction, RC5_Ctrl_t RC5_Ctrl)
{
  uint16_t star1 = 0x2000;
  uint16_t star2 = 0x1000;
  uint16_t addr = 0;

  while (RC5SendOpCompleteFlag == 0x00)
  {}

  /* Check if Instruction is 128-bit length */
  if (RC5_Instruction >= 64)
  {
    /* Reset field bit: command is 7-bit length */
    star2 = 0;
    /* Keep the lowest 6 bits of the command */
    RC5_Instruction &= 0x003F;
  }
  else /* Instruction is 64-bit length */
  {
    /* Set field bit: command is 6-bit length */
    star2 = 0x1000;
  }

  RC5SendOpReadyFlag = 0;
  RC5ManchesterFrameFormat = 0;
  RC5BinaryFrameFormat = 0;
  addr = ((uint16_t)(RC5_Address)) << 6;
  RC5BinaryFrameFormat =  (star1) | (star2) | (RC5_Ctrl) | (addr) | (RC5_Instruction);
  return (RC5BinaryFrameFormat);
}

/**
  * @brief Convert the RC5 frame from binary to Manchester Format.
  * @param RC5_BinaryFrameFormat : the RC5 frame in binary format.
  * @retval the RC5 frame in Manchester format.
  */
static uint32_t RC5_ManchesterConvert(uint16_t RC5_BinaryFrameFormat)
{
  uint8_t i = 0;
  uint16_t Mask = 1;
  uint16_t bit_format = 0;
  uint32_t ConvertedMsg = 0;

  for (i = 0; i < RC5RealFrameLength; i++)
  {
    bit_format = ((((uint16_t)(RC5_BinaryFrameFormat)) >> i) & Mask) << i;
    ConvertedMsg = ConvertedMsg << 2;

    if (bit_format != 0 ) /* Manchester 1 -|_  */
    {
      ConvertedMsg |= RC5HIGHSTATE;
    }
    else /* Manchester 0 _|-  */
    {
      ConvertedMsg |= RC5LOWSTATE;
    }
  }
  return (ConvertedMsg);
}

/**
  * @brief  Force TIM16 Channel 1 output to active or inactive state
  * @note   This controls the 38kHz carrier modulation (ON/OFF)
  * @param  action: TIM_FORCED_ACTIVE or TIM_FORCED_INACTIVE
  * @retval None
  */
void TIM_ForcedOC1Config(uint32_t action)
{
  /* Modify the Output Compare Mode for Channel 1 */
  uint32_t tmpccmrx = TimHandleHF.Instance->CCMR1;
  
  /* Reset the OC1M bits (bits 6:4 and bit 16) in the CCMR1 register */
  tmpccmrx &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1M_3);
  
  if (action == TIM_FORCED_ACTIVE)
  {
    /* Enable PWM mode to generate 38kHz carrier */
    tmpccmrx |= TIM_OCMODE_PWM1;  /* PWM mode 1 */
  }
  else
  {
    /* Force inactive level - carrier OFF */
    tmpccmrx |= TIM_OCMODE_FORCED_INACTIVE;
  }
  
  /* Write to TIMx CCMR1 register */
  TimHandleHF.Instance->CCMR1 = tmpccmrx;
}

/**
  * @brief  Timer period elapsed callback - called when TIM15 overflows
  * @note   This callback generates the RC5 Manchester encoded signal
  * @param  htim: Timer handle that triggered the interrupt
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* Check if interrupt is from TIM15 (bit timing timer) */
  if (htim->Instance == TIM15)
  {
    /* Generate next bit of RC5 signal */
    RC5_Encode_SignalGenerate();
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
