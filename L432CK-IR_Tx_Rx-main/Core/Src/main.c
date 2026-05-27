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
#include "ir_transceiver.h"
#include "rc5_decode.h"
#include "rc5_encode.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>  // NODIG voor atoi

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define PLAYER_NAME_MAX_LEN 16U
#define PLAYER_MAX_HITPOINTS 5U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
// Variabelen voor de IR-zender (RC5)
uint8_t toggle_bit = 0;
uint8_t tx_command = 0;    // Wordt aangepast via Bluetooth commando

// Button handling
static volatile uint8_t button_pressed = 0;
static uint32_t last_button_tick = 0;
#define DEBOUNCE_MS 300

// Bluetooth & Opdracht 3 variabelen
#define RX_BUF_SIZE 64
#define BLE_CMD_LINE_SIZE 128
#define BLE_TX_LINE_SIZE 512
#define BLE_TX_QUEUE_DEPTH 4
#define USART2_TX_LINE_SIZE 128U
#define USART2_DEBUG_QUEUE_DEPTH 4U
#define USART2_HUD_QUEUE_DEPTH 2U

uint8_t rx_buffer[RX_BUF_SIZE];      // De buffer waar DMA de Bluetooth data dumpt
uint32_t player_hits[32] = {0};      // Hit-tracking: index is het RC5 adres (0-31)

typedef struct
{
  char name[PLAYER_NAME_MAX_LEN + 1U];
  uint32_t team_color_rgb;
  uint8_t hitpoints;
  uint8_t max_hitpoints;
  uint8_t rc5_address;
} PlayerState_t;

static PlayerState_t player_state =
{
  "ALEX",
  0x001F00UL,
  PLAYER_MAX_HITPOINTS,
  PLAYER_MAX_HITPOINTS,
  0U
};

typedef struct
{
  char data[BLE_TX_LINE_SIZE];
  uint16_t length;
} BleTxSlot_t;

typedef struct
{
  char data[USART2_TX_LINE_SIZE];
  uint16_t length;
} Usart2TxSlot_t;

static uint16_t ble_dma_last_pos = 0;
static char ble_cmd_line[BLE_CMD_LINE_SIZE];
static uint16_t ble_cmd_line_len = 0;
static BleTxSlot_t ble_tx_queue[BLE_TX_QUEUE_DEPTH];
static uint8_t ble_tx_head = 0;
static uint8_t ble_tx_tail = 0;
static uint8_t ble_tx_count = 0;
static volatile uint8_t ble_tx_busy = 0;
static uint32_t ble_last_rx_tick = 0;
static Usart2TxSlot_t usart2_debug_queue[USART2_DEBUG_QUEUE_DEPTH];
static Usart2TxSlot_t usart2_hud_queue[USART2_HUD_QUEUE_DEPTH];
static uint8_t usart2_debug_head = 0U;
static uint8_t usart2_debug_tail = 0U;
static uint8_t usart2_debug_count = 0U;
static uint8_t usart2_hud_head = 0U;
static uint8_t usart2_hud_tail = 0U;
static uint8_t usart2_hud_count = 0U;
static volatile uint8_t usart2_tx_busy = 0U;
static volatile uint8_t usart2_tx_active_is_hud = 0U;
static char usart2_stdio_line[USART2_TX_LINE_SIZE];
static uint16_t usart2_stdio_len = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM15_Init(void);
static void MX_TIM16_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void ProcessBleUartRxDma(void);
static void BleUart_QueueFormatted(const char *fmt, ...);
static void BleUart_ServiceTx(void);
void send_debug(const char *fmt, ...);
void send_hud(const char *fmt, ...);
static void BleUart_HandleCommand(char *line);
static char *BleUart_Trim(char *text);
static uint8_t BleUart_ParseUnsignedArgument(const char *text, unsigned long *value);
static uint8_t BleUart_ParseQuotedTextArgument(const char *text, char *output, size_t output_size);
static uint8_t BleUart_ParseColorArgument(const char *text, uint32_t *value);
static void Player_ResetHitpoints(void);
static void Player_RegisterHit(void);
static void Player_ApplyDisplayState(void);
static void Player_SetName(const char *new_name);
static void Player_SetTeamColor(uint32_t new_color);
static void Player_SetRc5Address(uint8_t new_address);
static void Player_ApplyRc5Hit(void);
static void BleUart_FinalizeLine(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void BleUart_FinalizeLine(void)
{
  if (ble_cmd_line_len == 0U)
  {
    return;
  }

  ble_cmd_line[ble_cmd_line_len] = '\0';
  BleUart_HandleCommand(ble_cmd_line);
  ble_cmd_line_len = 0U;
}

static void ProcessBleUartRxDma(void)
{
  uint16_t dma_pos = (uint16_t)(RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));

  if (dma_pos == ble_dma_last_pos)
  {
    return;
  }

  while (ble_dma_last_pos != dma_pos)
  {
    uint8_t byte = rx_buffer[ble_dma_last_pos];

    if ((byte == '\r') || (byte == '\n') || (byte == '\0'))
    {
      BleUart_FinalizeLine();
    }
    else if (ble_cmd_line_len < (BLE_CMD_LINE_SIZE - 1U))
    {
      ble_cmd_line[ble_cmd_line_len++] = (char)byte;
      ble_last_rx_tick = HAL_GetTick();
    }
    else
    {
      ble_cmd_line_len = 0U;
      BleUart_QueueFormatted("ERROR command_too_long");
    }

    ble_dma_last_pos++;
    if (ble_dma_last_pos >= RX_BUF_SIZE)
    {
      ble_dma_last_pos = 0U;
    }
  }
}

static char *BleUart_Trim(char *text)
{
  while ((*text != '\0') && isspace((unsigned char)*text))
  {
    text++;
  }

  char *end = text + strlen(text);
  while ((end > text) && isspace((unsigned char)end[-1]))
  {
    end--;
  }

  *end = '\0';
  return text;
}

static uint8_t BleUart_ParseUnsignedArgument(const char *text, unsigned long *value)
{
  if ((text == NULL) || (value == NULL))
  {
    return 0U;
  }

  while ((*text != '\0') && isspace((unsigned char)*text))
  {
    text++;
  }

  if (*text == '"')
  {
    text++;
  }

  char *endptr = NULL;
  unsigned long parsed = strtoul(text, &endptr, 0);
  if (endptr == text)
  {
    return 0U;
  }

  if (*endptr == '"')
  {
    endptr++;
  }

  while ((*endptr != '\0') && isspace((unsigned char)*endptr))
  {
    endptr++;
  }

  if (*endptr != '\0')
  {
    return 0U;
  }

  *value = parsed;
  return 1U;
}

static uint8_t BleUart_ParseTextArgument(const char *text, char *output, size_t output_size)
{
  if ((text == NULL) || (output == NULL) || (output_size == 0U))
  {
    return 0U;
  }

  while ((*text != '\0') && isspace((unsigned char)*text))
  {
    text++;
  }

  size_t length = 0U;
  if (*text == '"')
  {
    text++;

    while ((text[length] != '\0') && (text[length] != '"'))
    {
      if (length >= (output_size - 1U))
      {
        return 0U;
      }

      length++;
    }

    if (text[length] != '"')
    {
      return 0U;
    }
  }
  else
  {
    const char *end = text + strlen(text);
    while ((end > text) && isspace((unsigned char)end[-1]))
    {
      end--;
    }

    length = (size_t)(end - text);
    if ((length == 0U) || (length >= output_size))
    {
      return 0U;
    }
  }

  memcpy(output, text, length);
  output[length] = '\0';

  text += length;
  if (*text == '"')
  {
    text++;
  }

  while ((*text != '\0') && isspace((unsigned char)*text))
  {
    text++;
  }

  return (*text == '\0') ? 1U : 0U;
}

static uint8_t BleUart_ParseColorArgument(const char *text, uint32_t *value)
{
  if ((text == NULL) || (value == NULL))
  {
    return 0U;
  }

  while ((*text != '\0') && isspace((unsigned char)*text))
  {
    text++;
  }

  if (*text == '"')
  {
    text++;
  }

  if (*text == '#')
  {
    text++;
  }
  else if ((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X')))
  {
    text += 2U;
  }

  if (!isxdigit((unsigned char)*text))
  {
    return 0U;
  }

  char *endptr = NULL;
  unsigned long parsed = strtoul(text, &endptr, 16);
  if (endptr == text)
  {
    return 0U;
  }

  if (*endptr == '"')
  {
    endptr++;
  }

  while ((*endptr != '\0') && isspace((unsigned char)*endptr))
  {
    endptr++;
  }

  if (*endptr != '\0')
  {
    return 0U;
  }

  *value = (uint32_t)(parsed & 0x00FFFFFFUL);
  return 1U;
}

static void Player_ApplyDisplayState(void)
{
  send_hud("HUD:NAME:%s", player_state.name);
  send_hud("HUD:COLOR:#%06lX", (unsigned long)player_state.team_color_rgb);
  send_hud("HUD:HP:%u/%u", player_state.hitpoints, player_state.max_hitpoints);
  send_hud("HUD:ADDRESS:%u", player_state.rc5_address);
}

static void Player_SetName(const char *new_name)
{
  if (new_name == NULL)
  {
    return;
  }

  strncpy(player_state.name, new_name, sizeof(player_state.name) - 1U);
  player_state.name[sizeof(player_state.name) - 1U] = '\0';
  Player_ApplyDisplayState();
}

static void Player_SetTeamColor(uint32_t new_color)
{
  player_state.team_color_rgb = (new_color & 0x00FFFFFFUL);
  Player_ApplyDisplayState();
}

static void Player_SetRc5Address(uint8_t new_address)
{
  player_state.rc5_address = (uint8_t)(new_address & 0x1FU);
  Player_ApplyDisplayState();
}

static void Player_ApplyRc5Hit(void)
{
  if (player_state.hitpoints > 0U)
  {
    player_state.hitpoints--;
  }

  Player_ApplyDisplayState();
}

static void Player_ResetHitpoints(void)
{
  player_state.hitpoints = player_state.max_hitpoints;
  Player_ApplyDisplayState();
}

static void Player_RegisterHit(void)
{
  Player_ApplyRc5Hit();
}

static void BleUart_ServiceTx(void)
{
  if (ble_tx_busy || (ble_tx_count == 0U))
  {
    return;
  }

  if (HAL_UART_Transmit_DMA(&huart1,
                            (uint8_t *)ble_tx_queue[ble_tx_tail].data,
                            ble_tx_queue[ble_tx_tail].length) == HAL_OK)
  {
    ble_tx_busy = 1U;
  }
}

static void BleUart_QueueFormatted(const char *fmt, ...)
{
  if (fmt == NULL)
  {
    return;
  }

  if (ble_tx_count >= BLE_TX_QUEUE_DEPTH)
  {
    return;
  }

  BleTxSlot_t *slot = &ble_tx_queue[ble_tx_head];
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(slot->data, BLE_TX_LINE_SIZE - 3U, fmt, args);
  va_end(args);

  if (written < 0)
  {
    return;
  }

  if (written > (int)(BLE_TX_LINE_SIZE - 3U))
  {
    written = (int)(BLE_TX_LINE_SIZE - 3U);
  }

  slot->data[written++] = '\r';
  slot->data[written++] = '\n';
  slot->data[written] = '\0';
  slot->length = (uint16_t)written;

  ble_tx_head = (uint8_t)((ble_tx_head + 1U) % BLE_TX_QUEUE_DEPTH);
  ble_tx_count++;
  BleUart_ServiceTx();
}

static void Usart2_ServiceTx(void)
{
  Usart2TxSlot_t *slot = NULL;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (usart2_tx_busy != 0U)
  {
    __set_PRIMASK(primask);
    return;
  }

  if (usart2_hud_count > 0U)
  {
    slot = &usart2_hud_queue[usart2_hud_tail];
    usart2_tx_active_is_hud = 1U;
  }
  else if (usart2_debug_count > 0U)
  {
    slot = &usart2_debug_queue[usart2_debug_tail];
    usart2_tx_active_is_hud = 0U;
  }
  else
  {
    __set_PRIMASK(primask);
    return;
  }

  usart2_tx_busy = 1U;
  __set_PRIMASK(primask);

  if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)slot->data, slot->length) != HAL_OK)
  {
    primask = __get_PRIMASK();
    __disable_irq();
    usart2_tx_busy = 0U;
    __set_PRIMASK(primask);
  }
}

static void Usart2_TxComplete(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (usart2_tx_active_is_hud != 0U)
  {
    if (usart2_hud_count > 0U)
    {
      usart2_hud_tail = (uint8_t)((usart2_hud_tail + 1U) % USART2_HUD_QUEUE_DEPTH);
      usart2_hud_count--;
    }
  }
  else if (usart2_debug_count > 0U)
  {
    usart2_debug_tail = (uint8_t)((usart2_debug_tail + 1U) % USART2_DEBUG_QUEUE_DEPTH);
    usart2_debug_count--;
  }

  usart2_tx_busy = 0U;
  __set_PRIMASK(primask);

  Usart2_ServiceTx();
}

static void Usart2_QueueFormatted(uint8_t is_hud, const char *fmt, va_list args)
{
  char line[USART2_TX_LINE_SIZE];
  int written;

  if (fmt == NULL)
  {
    return;
  }

  written = vsnprintf(line, sizeof(line), fmt, args);

  if (written < 0)
  {
    return;
  }

  if (written >= (int)sizeof(line))
  {
    written = (int)sizeof(line) - 1;
  }

  while ((written > 0) && ((line[written - 1] == '\n') || (line[written - 1] == '\r')))
  {
    written--;
  }

  if (written <= 0)
  {
    return;
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (is_hud != 0U)
  {
    if (usart2_hud_count >= USART2_HUD_QUEUE_DEPTH)
    {
      usart2_hud_tail = (uint8_t)((usart2_hud_tail + 1U) % USART2_HUD_QUEUE_DEPTH);
      usart2_hud_count--;
    }

    Usart2TxSlot_t *slot = &usart2_hud_queue[usart2_hud_head];
    memcpy(slot->data, line, (size_t)written);
    slot->data[written++] = '\r';
    slot->data[written++] = '\n';
    slot->data[written] = '\0';
    slot->length = (uint16_t)written;

    usart2_hud_head = (uint8_t)((usart2_hud_head + 1U) % USART2_HUD_QUEUE_DEPTH);
    usart2_hud_count++;
  }
  else
  {
    if (usart2_debug_count >= USART2_DEBUG_QUEUE_DEPTH)
    {
      __set_PRIMASK(primask);
      return;
    }

    Usart2TxSlot_t *slot = &usart2_debug_queue[usart2_debug_head];
    memcpy(slot->data, line, (size_t)written);
    slot->data[written++] = '\r';
    slot->data[written++] = '\n';
    slot->data[written] = '\0';
    slot->length = (uint16_t)written;

    usart2_debug_head = (uint8_t)((usart2_debug_head + 1U) % USART2_DEBUG_QUEUE_DEPTH);
    usart2_debug_count++;
  }

  __set_PRIMASK(primask);
  Usart2_ServiceTx();
}

void send_debug(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  Usart2_QueueFormatted(0U, fmt, args);
  va_end(args);
}

void send_hud(const char *fmt, ...)
{
  va_list args;
  va_list args_copy;
  char buf[BLE_TX_LINE_SIZE];

  va_start(args, fmt);
  /* create a copy for formatting so we can forward to both queues */
  va_copy(args_copy, args);

  /* format into a stable buffer */
  int written = vsnprintf(buf, sizeof(buf), fmt, args_copy);
  (void)written;
  va_end(args_copy);

  /* queue for USART2 (HUD channel) using existing va_list API */
  Usart2_QueueFormatted(1U, fmt, args);
  va_end(args);

  /* also send the same line to the BLE/UART1 transmit queue so bridges/TFTs receive it */
  BleUart_QueueFormatted("%s", buf);
}

static void BleUart_HandleCommand(char *line)
{
  char *command = BleUart_Trim(line);

  if (*command == '\0')
  {
    return;
  }

  if (strcmp(command, "current_settings") == 0)
  {
    BleUart_QueueFormatted("current_settings address=%u command=%u", player_state.rc5_address, tx_command);
    return;
  }

  if ((strcmp(command, "current_hits") == 0) || (strcmp(command, "hits") == 0))
  {
    char response[BLE_TX_LINE_SIZE];
    int offset = snprintf(response, sizeof(response), "current_hits");

    for (uint8_t address = 0U; address < 32U; address++)
    {
      if ((offset < 0) || (offset >= (int)sizeof(response)))
      {
        break;
      }

      offset += snprintf(&response[offset], sizeof(response) - (size_t)offset,
                         " %u=%lu", address, (unsigned long)player_hits[address]);
    }

    BleUart_QueueFormatted("%s", response);
    return;
  }

  if ((strcmp(command, "reset_hits") == 0) || (strcmp(command, "RESET") == 0) || (strcmp(command, "reset") == 0))
  {
    memset(player_hits, 0, sizeof(player_hits));
    Player_ResetHitpoints();
    BleUart_QueueFormatted("reset_hits OK");
    return;
  }

  if ((strncmp(command, "set_name:", 9U) == 0) || (strncmp(command, "NAME:", 5U) == 0) || (strncmp(command, "name:", 5U) == 0))
  {
    char new_name[PLAYER_NAME_MAX_LEN + 1U];
    const char *value = (command[0] == 's') ? (command + 9U) : (command + 5U);
    if (!BleUart_ParseTextArgument(value, new_name, sizeof(new_name)))
    {
      BleUart_QueueFormatted("ERROR invalid_name");
      return;
    }

    Player_SetName(new_name);
    BleUart_QueueFormatted("set_name OK %s", player_state.name);
    return;
  }

  if ((strncmp(command, "set_team_color:", 15U) == 0) || (strncmp(command, "COLOR:", 6U) == 0) || (strncmp(command, "color:", 6U) == 0))
  {
    uint32_t new_color = 0U;
    const char *value = (command[0] == 's') ? (command + 15U) : (command + 6U);
    if (!BleUart_ParseColorArgument(value, &new_color))
    {
      BleUart_QueueFormatted("ERROR invalid_color");
      return;
    }

    Player_SetTeamColor(new_color);
    BleUart_QueueFormatted("set_team_color OK #%06lX", (unsigned long)player_state.team_color_rgb);
    return;
  }

  if ((strcmp(command, "simulate_hit") == 0) || (strcmp(command, "HIT") == 0) || (strcmp(command, "hit") == 0))
  {
    Player_RegisterHit();
    BleUart_QueueFormatted("simulate_hit OK hp=%u", player_state.hitpoints);
    return;
  }

  if ((strncmp(command, "set_address:", 12U) == 0) || (strncmp(command, "ADDRESS:", 8U) == 0) || (strncmp(command, "address:", 8U) == 0))
  {
    unsigned long value = 0U;
    const char *value_text = (command[0] == 's') ? (command + 12U) : (command + 8U);
    if (!BleUart_ParseUnsignedArgument(value_text, &value) || (value > 31UL))
    {
      BleUart_QueueFormatted("ERROR invalid_address");
      return;
    }

    Player_SetRc5Address((uint8_t)value);
    BleUart_QueueFormatted("set_address OK %u", player_state.rc5_address);
    return;
  }

  if ((strncmp(command, "set_command:", 12U) == 0) || (strncmp(command, "COMMAND:", 8U) == 0) || (strncmp(command, "command:", 8U) == 0))
  {
    unsigned long value = 0U;
    const char *value_text = (command[0] == 's') ? (command + 12U) : (command + 8U);
    if (!BleUart_ParseUnsignedArgument(value_text, &value) || (value > 63UL))
    {
      BleUart_QueueFormatted("ERROR invalid_command");
      return;
    }

    tx_command = (uint8_t)value;
    BleUart_QueueFormatted("set_command OK %u", tx_command);
    return;
  }

  BleUart_QueueFormatted("ERROR unknown_command");
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_3)  /* PA3 — BTN_TX */
  {
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) != GPIO_PIN_SET) return;

    uint32_t now = HAL_GetTick();
    if ((now - last_button_tick) < DEBOUNCE_MS) return;
    last_button_tick = now;

    if (button_pressed) return;
    button_pressed = 1;
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
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_TIM15_Init();
  MX_TIM16_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  IR_Transceiver_Init();

  /* --- OPDRACHT 1: Start DMA voor LilyGO (huart1) --- */

  // 1. Start de cirkelvormige ontvangst. 
  // De STM32 staat nu klaar om bytes van de ESP32 te ontvangen in de rx_buffer.
  HAL_UART_Receive_DMA(&huart1, rx_buffer, RX_BUF_SIZE);
  ble_dma_last_pos = 0;
  ble_last_rx_tick = HAL_GetTick();

  // 2. Optioneel: Stuur een opstartbericht naar de telefoon (als de app al verbonden is)
  BleUart_QueueFormatted("Hello BLE");
  Player_ApplyDisplayState();


  send_debug("USART2 TX DMA ready");
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    ProcessBleUartRxDma();
    if ((ble_cmd_line_len > 0U) && ((HAL_GetTick() - ble_last_rx_tick) >= 80U))
    {
      BleUart_FinalizeLine();
    }
    BleUart_ServiceTx();

    /* --- State machine processing --- */
    IR_Transceiver_Process();

    /* --- TX trigger (Schieten) --- */
    if (button_pressed && IR_GetState() == IR_STATE_IDLE)
    {
      IR_StartTransmit(toggle_bit, player_state.rc5_address, tx_command); 
        
        toggle_bit ^= 1;
        button_pressed = 0;

        // Debug bericht naar PC
        send_debug("[TX] Addr:0x%02X Cmd:0x%02X", player_state.rc5_address, tx_command);
    }

    /* --- RX frame received (Geraakt worden) --- */
    if (RC5FrameReceived && IR_GetState() == IR_STATE_IDLE)
    {
        RC5_Decode(&RC5_FRAME);

        // Update de hit-counter voor dit adres
        if (RC5_FRAME.Address < 32) {
            player_hits[RC5_FRAME.Address]++;
        }

        if (RC5_FRAME.Address == player_state.rc5_address)
        {
          Player_RegisterHit();
        }

        // Toon info in de seriële monitor van je PC
          send_debug("[RX] Hit van Addr:0x%02X | Totaal hits: %lu | HP: %u",
            RC5_FRAME.Address, player_hits[RC5_FRAME.Address], player_state.hitpoints);

        /* LED knipper bij hit */
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
        
        RC5FrameReceived = 0; 
    }

    BleUart_ServiceTx();
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 31;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3700;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
  sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
  sSlaveConfig.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sSlaveConfig.TriggerPrescaler = TIM_ICPSC_DIV1;
  sSlaveConfig.TriggerFilter = 0;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* Re-apply slave reset mode AFTER IC channel config, because
     HAL_TIM_IC_ConfigChannel on CH1 can clobber SMCR. */
  {
    TIM_SlaveConfigTypeDef sSlaveRe = {0};
    sSlaveRe.SlaveMode        = TIM_SLAVEMODE_RESET;
    sSlaveRe.InputTrigger     = TIM_TS_TI1FP1;
    sSlaveRe.TriggerPolarity  = TIM_TRIGGERPOLARITY_FALLING;
    sSlaveRe.TriggerPrescaler = TIM_ICPSC_DIV1;
    sSlaveRe.TriggerFilter    = 0;
    HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveRe);
  }

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM15 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM15_Init(void)
{

  /* USER CODE BEGIN TIM15_Init 0 */

  /* USER CODE END TIM15_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM15_Init 1 */

  /* USER CODE END TIM15_Init 1 */
  htim15.Instance = TIM15;
  htim15.Init.Prescaler = 31;
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 888;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim15, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM15_Init 2 */

  /* USER CODE END TIM15_Init 2 */

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 0;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 842;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 210;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */
  HAL_TIM_MspPostInit(&htim16);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (ble_tx_count > 0U)
    {
      ble_tx_tail = (uint8_t)((ble_tx_tail + 1U) % BLE_TX_QUEUE_DEPTH);
      ble_tx_count--;
    }

    ble_tx_busy = 0U;
  }
  else if (huart->Instance == USART2)
  {
    Usart2_TxComplete();
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    ble_tx_busy = 0U;
  }
  else if (huart->Instance == USART2)
  {
    Usart2_TxComplete();
  }
}

int __io_putchar(int ch)
{
  if (ch == '\r')
  {
    return ch;
  }

  if (ch == '\n')
  {
    if (usart2_stdio_len > 0U)
    {
      usart2_stdio_line[usart2_stdio_len] = '\0';
      send_debug("%s", usart2_stdio_line);
      usart2_stdio_len = 0U;
    }

    return ch;
  }

  if (usart2_stdio_len >= (USART2_TX_LINE_SIZE - 1U))
  {
    usart2_stdio_line[usart2_stdio_len] = '\0';
    send_debug("%s", usart2_stdio_line);
    usart2_stdio_len = 0U;
  }

  usart2_stdio_line[usart2_stdio_len++] = (char)ch;
  return ch;
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
