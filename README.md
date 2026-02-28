# Lasertag-Game-opdracht-1

## Fase 1: IR Transmitter Implementation

### Project Overview
This project implements an RC5 infrared remote control transmitter on the STM32L432KC Nucleo board. The implementation generates a 38kHz carrier wave modulated with RC5 protocol data for laser tag applications.

---

## Hardware Configuration

### Microcontroller
- **Board**: NUCLEO-L432KC
- **MCU**: STM32L432KC (Cortex-M4, 80MHz)
- **Clock Configuration**: 80MHz system clock from PLL

### Pin Connections

| Pin   | Function          | Description                          |
|-------|-------------------|--------------------------------------|
| PA6   | TIM16_CH1 (AF14)  | IR LED driver (38kHz carrier output) |
| PA2   | TIM15_CH1 (AF14)  | Data envelope (for debugging)        |
| PB4   | GPIO Input        | Button input (active LOW, pull-up)   |
| PB3   | GPIO Output (LD3) | LED indicator (blinks on transmit)   |

### IR LED Circuit
Connect the IR LED according to AN4834 Figure 8:

```
PA6 (TIM16_CH1) ----[Resistor]----[Transistor]----[IR LED]----GND
                                      |
                                     GND
```

**Recommended Components:**
- IR LED: TSAL6200 or similar (940nm)
- Transistor: 2N2222 or BC547
- Resistor: Calculate based on LED datasheet (typically 100-220Ω)

**LED Current Calculation:**
```
Assuming:
- IR LED forward voltage (Vf) = 1.2V
- Desired current (If) = 100mA
- MCU output voltage = 3.3V
- Transistor Vce(sat) ≈ 0.2V

Resistor = (3.3V - Vce(sat) - Vf) / If
Resistor = (3.3 - 0.2 - 1.2) / 0.1
Resistor = 19Ω (use 22Ω standard value)

For safety, use 100Ω for lower current (~20mA)
```

---

## Software Configuration

### Timer Configuration

#### TIM16 - 38kHz Carrier Generator
- **Purpose**: Generates the 38kHz carrier wave for IR transmission
- **Configuration**:
  - Prescaler: 1 (Timer clock = 80MHz / 2 = 40MHz)
  - Period (ARR): 1053 (40MHz / 1053 = 38.005 kHz)
  - Pulse (CCR): 263 (25% duty cycle)
  - Mode: PWM Mode 1
  - Output: PA6 (TIM16_CH1, AF14)

**Calculations:**
```
Timer Clock = 80MHz / (PSC + 1) = 80MHz / 2 = 40MHz
Frequency = Timer Clock / (ARR + 1) = 40MHz / 1053 = 38.005 kHz
Duty Cycle = CCR / ARR = 263 / 1053 = 24.98% ≈ 25%
```

#### TIM15 - RC5 Bit Timing
- **Purpose**: Controls RC5 bit timing (889µs per bit)
- **Configuration**:
  - Prescaler: 1 (Timer clock = 40MHz)
  - Period (ARR): 35556 (889µs bit period)
  - Mode: PWM with interrupt on overflow
  - Output: PA2 (TIM15_CH1, AF14) - optional for debugging

**Calculations:**
```
Timer Clock = 80MHz / (PSC + 1) = 80MHz / 2 = 40MHz
Bit Period = ARR / Timer Clock = 35556 / 40MHz = 0.8889 ms = 889µs
```

### RC5 Protocol
- **Frame Format**: 14 bits (2 start bits + 1 toggle bit + 5 address bits + 6 command bits)
- **Encoding**: Manchester encoding
- **Bit Duration**: 889µs per bit (1.778ms per Manchester symbol)
- **Frame Duration**: ~25ms

---

## Usage

### Building and Flashing
1. Open the project in Keil µVision or STM32CubeIDE
2. Build the project
3. Flash to the NUCLEO-L432KC board

### Operation
1. Connect a button between PB4 and GND
2. Connect the IR LED circuit to PA6
3. Power the board
4. Press the button to send an RC5 frame
5. LD3 LED will blink during transmission

### Default RC5 Frame
- **Address**: 0 (TV device)
- **Command**: 12 (Volume Up)
- **Control Bit**: Reset

### Monitoring with Oscilloscope

#### Measuring 38kHz Carrier (PA6):
1. Connect oscilloscope probe to PA6
2. Press the button to trigger transmission
3. **Expected waveform**:
   - Frequency: 38 kHz (period = 26.3µs)
   - Duty cycle: 25%
   - Bursts of carrier modulated by RC5 data

#### Measuring RC5 Data Envelope (PA2):
1. Connect oscilloscope probe to PA2
2. Press the button to trigger transmission
3. **Expected waveform**:
   - Manchester encoded data
   - Bit period: 889µs
   - Total frame: ~25ms

---

## Code Structure

### Main Files
- **main.c**: Main application logic, button handling, LED control
- **rc5_encode.c**: RC5 encoding and signal generation
- **rc5_encode.h**: RC5 encoder API
- **ir_common.h**: Common IR definitions and timer configurations
- **stm32l4xx_it.c**: Interrupt handlers (TIM15 interrupt)
- **stm32l4xx_hal_msp.c**: HAL MSP initialization (GPIO, timers)

### Key Functions

#### Initialization
```c
RC5_Encode_Init(void);  // Initialize timers and GPIO for IR transmission
```

#### Sending RC5 Frame
```c
RC5_Encode_SendFrame(uint8_t address, uint8_t command, RC5_Ctrl_t ctrl);
// Example: RC5_Encode_SendFrame(0, 12, RC5_CTRL_RESET);
```

---

## Testing & Verification

### Test Points for Report

1. **38kHz Carrier Waveform**
   - Measure on PA6 with oscilloscope
   - Verify frequency: 38 kHz ± 1%
   - Verify duty cycle: 25% ± 2%

2. **RC5 Bit Timing**
   - Measure on PA2 with oscilloscope
   - Verify bit period: 889µs ± 10µs
   - Identify Manchester encoding patterns

3. **RC5 Frame Structure**
   - Capture complete frame (~25ms)
   - Decode start bits, toggle bit, address, and command
   - Verify frame format matches RC5 specification

4. **LED Current Calculation**
   - Document resistor value selection
   - Calculate LED current from datasheet
   - Verify transistor operates in saturation region

5. **Button Response**
   - Press button → LED blinks → IR transmission
   - Verify debouncing (50ms delay between presses)

---

## Troubleshooting

### No IR Output
- Check PA6 connection
- Verify TIM16 is running (oscilloscope on PA6)
- Check button connection (PB4 to GND)

### Wrong Frequency
- Verify system clock is 80MHz
- Check TIM16 prescaler and period values
- Recalculate if using different clock

### No Button Response
- Check button connection (active LOW)
- Verify internal pull-up is enabled on PB4
- Check debouncing delay

### LED Not Blinking
- Check PB3 connection (LD3)
- Verify GPIO initialization

---

## References
- **AN4834**: Using the hardware real-time clock (RTC) and the Tamper management unit (TAMU) of STM32 microcontrollers
- **RC5 Protocol**: Philips RC5 infrared remote control protocol specification
- **STM32L432KC Datasheet**: STMicroelectronics
- **X-CUBE-IRREMOTE**: STMicroelectronics IR remote control middleware

---

## Authors
- Vives Hogeschool - Game Technology
- Fase 2 - Semester 2

## License
Educational use only
