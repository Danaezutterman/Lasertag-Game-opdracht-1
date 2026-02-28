# IR Transmitter Quick Reference Card

## 📌 Pin Configuration Summary

| Pin  | Function        | Description                    | Notes                  |
|------|-----------------|--------------------------------|------------------------|
| PA6  | TIM16_CH1       | **IR LED Output** (38kHz)      | Connect to transistor  |
| PA2  | TIM15_CH1       | Data envelope (debug)          | Optional oscilloscope  |
| PB4  | Button Input    | Trigger IR transmission        | Active LOW, pull-up    |
| PB3  | LED (LD3)       | Transmission indicator         | Onboard LED            |

---

## ⚙️ Timer Settings

### TIM16 (38kHz Carrier)
```
Frequency: 38 kHz
Duty Cycle: 25%
Prescaler: 1
Period (ARR): 1053
Pulse (CCR): 263
```

### TIM15 (RC5 Bit Timing)
```
Bit Period: 889 µs
Prescaler: 1
Period (ARR): 35556
```

---

## 🔧 Hardware Setup

### IR LED Circuit
```
PA6 ──[10kΩ]──┐
              │
            ┌─┴─┐ BC547
            │ B │ (NPN)
            │ C │
            └─┬─┘
              │
         [82Ω]│
              │
           IR LED (940nm)
              │
             GND
```

### Button Connection
```
   +3.3V (internal pull-up)
      │
     PB4 ──────[Button]──── GND
```

---

## 💻 Software API

### Initialize IR Transmitter
```c
RC5_Encode_Init();
```

### Send RC5 Frame
```c
// Send command
RC5_Encode_SendFrame(address, command, RC5_CTRL_RESET);

// Example: Send Volume Up (command 12) to TV (address 0)
RC5_Encode_SendFrame(0, 12, RC5_CTRL_RESET);
```

### Common RC5 Commands (TV - Address 0)
| Command | Code | Description  |
|---------|------|--------------|
| Power   | 12   | Power toggle |
| Vol+    | 16   | Volume up    |
| Vol-    | 17   | Volume down  |
| CH+     | 32   | Channel up   |
| CH-     | 33   | Channel down |
| Mute    | 13   | Mute toggle  |

---

## 📊 Oscilloscope Measurements

### 38kHz Carrier (PA6)
```
Time base: 10 µs/div
Voltage: 1 V/div
Trigger: Rising edge

Expected:
- Period: 26.3 µs
- Frequency: 38 kHz
- Duty Cycle: 25%
```

### RC5 Data (PA2)
```
Time base: 2 ms/div
Voltage: 1 V/div
Trigger: Rising edge

Expected:
- Bit period: 889 µs
- Frame duration: ~25 ms
- Manchester encoding visible
```

---

## 🧪 Testing Procedure

1. **Build and Flash**
   - Open project in Keil µVision
   - Build project (F7)
   - Flash to board (F8)

2. **Connect Hardware**
   - Button between PB4 and GND
   - IR LED circuit on PA6
   - Oscilloscope to PA6 (carrier) and PA2 (data)

3. **Test Operation**
   - Press button
   - LD3 should blink
   - Check carrier on PA6 (38 kHz)
   - Check data on PA2 (Manchester)

4. **Verify Measurements**
   - Carrier frequency: 38 kHz ± 5%
   - Duty cycle: 25% ± 10%
   - Bit timing: 889 µs ± 5%

---

## 🐛 Troubleshooting

| Problem                | Possible Cause              | Solution                        |
|------------------------|-----------------------------|---------------------------------|
| No carrier on PA6      | Timer not initialized       | Check RC5_Encode_Init()         |
| Wrong frequency        | Clock configuration error   | Verify 80 MHz system clock      |
| Button not responding  | Wrong pin or pull-up        | Check PB4 connection            |
| LED not blinking       | Wrong GPIO pin              | Verify PB3 connection           |
| No IR detection        | LED circuit issue           | Check transistor, LED polarity  |

---

## 📝 Code Snippets

### Change RC5 Command
```c
// In main.c, modify these variables:
rc5_address = 0;     // Device address (0-31)
rc5_command = 12;    // Command code (0-127)
```

### Adjust Transmission Delay
```c
// In main.c, in the while loop:
HAL_Delay(25);  // Increase for more visible LED blink
```

### Debug Output (Optional - requires UART)
```c
// Add this in main.c after RC5_Encode_SendFrame()
printf("Sent: Addr=%d, Cmd=%d\n", rc5_address, rc5_command);
```

---

## 📚 Important Formulas

### Carrier Frequency
```
f = Timer_Clock / (ARR + 1)
f = 40 MHz / 1053 = 38.005 kHz
```

### Duty Cycle
```
Duty = (CCR / ARR) × 100%
Duty = (263 / 1053) × 100% = 24.98%
```

### Bit Period
```
T_bit = ARR / Timer_Clock
T_bit = 35556 / 40 MHz = 889 µs
```

### LED Current
```
I_LED = (V_supply - V_f - V_ce(sat)) / R_LED
I_LED = (5V - 1.2V - 0.2V) / 82Ω ≈ 44 mA
```

---

## 🎯 Report Checklist

- [ ] Circuit schematic with all component values
- [ ] LED current calculation from datasheets
- [ ] Oscilloscope screenshot: 38kHz carrier
- [ ] Oscilloscope screenshot: 25% duty cycle measurement
- [ ] Oscilloscope screenshot: RC5 data bits with annotations
- [ ] Bit identification (start, toggle, address, command)
- [ ] Verification: LED blinks on button press
- [ ] All calculations shown with units

---

## 🔗 Quick Links

- **Project Folder**: `Lasertag-Game opdracht1/`
- **Main Code**: `Core/Src/main.c`
- **RC5 Encoder**: `Core/Src/rc5_encode.c`
- **Documentation**: `README.md`, `OSCILLOSCOPE_MEASUREMENTS.md`

---

## 📞 Key Registers (For Debugging)

```c
// Check if timers are running
TIM16->CR1 & TIM_CR1_CEN  // Should be 1 (TIM16 enabled)
TIM15->CR1 & TIM_CR1_CEN  // 1 when transmitting, 0 when idle

// Check timer configuration
TIM16->ARR   // Should be 1053
TIM16->CCR1  // Should be 263
TIM15->ARR   // Should be 35556

// Check GPIO configuration
GPIOA->MODER // PA6 should be in AF mode (0b10)
GPIOA->AFR[0] // PA6 AF14 (TIM16)
```

---

*Quick Reference - VIVES Hogeschool - Game Technology*
*For detailed information, see README.md and OSCILLOSCOPE_MEASUREMENTS.md*
