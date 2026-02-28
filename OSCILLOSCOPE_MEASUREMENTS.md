# Oscilloscope Measurements Guide - IR Transmitter

## Measurement Setup

### Equipment Needed
- Digital oscilloscope (minimum 100 MHz bandwidth)
- Oscilloscope probes (10x)
- NUCLEO-L432KC with IR transmitter firmware
- Button connected to PB4
- IR LED circuit connected to PA6

---

## 1. Measuring the 38kHz Carrier Wave

### Test Point: PA6 (TIM16_CH1)

### Oscilloscope Settings
- **Time Base**: 10-20 µs/div (to see 2-3 periods)
- **Voltage**: 1V/div
- **Coupling**: DC
- **Trigger**: Edge trigger, rising edge

### Procedure
1. Connect oscilloscope probe to PA6
2. Connect probe ground to board GND
3. Press button to trigger IR transmission
4. Capture stable waveform

### Expected Results
- **Frequency**: 38 kHz (period = 26.3 µs)
- **Duty Cycle**: 25%
- **Peak Voltage**: ~3.3V
- **Waveform**: Square wave bursts (on during Manchester '1', off during '0')

### Measurements to Record
```
Period (T) = _______ µs
Frequency (f) = 1/T = _______ kHz

High Time (Ton) = _______ µs
Low Time (Toff) = _______ µs

Duty Cycle = (Ton / T) × 100% = _______ %

Peak Voltage = _______ V
```

### Sample Calculation
```
Theoretical:
Period = 1 / 38 kHz = 26.3 µs
High Time = 26.3 µs × 0.25 = 6.58 µs
Low Time = 26.3 µs × 0.75 = 19.74 µs
Duty Cycle = 25%
```

### Screenshot Checklist
- [ ] Full carrier burst showing multiple periods
- [ ] Zoomed view of 2-3 carrier periods
- [ ] Cursor measurements showing period and duty cycle
- [ ] Clear amplitude measurements

---

## 2. Measuring the RC5 Data Envelope

### Test Point: PA2 (TIM15_CH1 - Data Visualization)

### Oscilloscope Settings
- **Time Base**: 1-2 ms/div (to see several bits)
- **Voltage**: 1V/div
- **Coupling**: DC
- **Trigger**: Edge trigger, rising edge

### Procedure
1. Connect oscilloscope probe to PA2
2. Connect probe ground to board GND
3. Set persistent display mode
4. Press button to trigger IR transmission
5. Capture complete frame

### Expected Results
- **Bit Period**: 889 µs per Manchester bit
- **Frame Duration**: ~25 ms (14 bits × 1.778 ms)
- **Encoding**: Manchester (each logical bit = two physical transitions)

### Measurements to Record
```
Single Bit Period = _______ µs
Half-Bit Period = _______ µs
Complete Frame Duration = _______ ms

Number of bits visible = _______ bits
```

### Manchester Encoding Identification

**Manchester '1'**: High-to-Low transition (₁₀)
```
|‾‾‾‾‾‾‾|_______|
  889µs total
```

**Manchester '0'**: Low-to-High transition (₀₁)
```
|_______|‾‾‾‾‾‾‾|
  889µs total
```

### Decoding Example
```
RC5 Frame Format (14 bits):
┌──┬──┬──┬─────────────┬───────────────────┐
│S1│S2│T │  Address    │     Command       │
└──┴──┴──┴─────────────┴───────────────────┘
 2   2  1      5 bits         6 bits

S1, S2 = Start bits (always '1')
T = Toggle bit (alternates on each new button press)
Address = Device address (5 bits)
Command = Command code (6 bits)
```

### Screenshot Checklist
- [ ] Complete frame showing all 14 bits
- [ ] Zoomed view showing 2-3 Manchester bits
- [ ] Cursor measurements showing bit period (889 µs)
- [ ] Annotated bits showing start, toggle, address, command

---

## 3.Combined View - Carrier Modulation

### Test Points: PA6 (carrier) and PA2 (data)

### Oscilloscope Settings
- **Channel 1**: PA6 (carrier) - 1V/div
- **Channel 2**: PA2 (data envelope) - 1V/div
- **Time Base**: 2-5 ms/div (to see modulation)
- **Trigger**: Channel 2, edge trigger

### Procedure
1. Connect CH1 probe to PA6
2. Connect CH2 probe to PA2
3. Use dual channel display
4. Trigger on PA2 (data envelope)
5. Capture synchronized waveforms

### Expected Results
- PA2 (data) shows Manchester encoding
- PA6 (carrier) shows 38kHz bursts only when PA2 is HIGH
- Clear correlation between data envelope and carrier modulation

### Screenshot Checklist
- [ ] Dual trace showing both channels
- [ ] Clear modulation relationship
- [ ] Annotated showing "carrier ON" and "carrier OFF" periods

---

## 4. IR LED Drive Circuit

### Component Selection

#### IR LED Specifications (example: TSAL6200)
```
Forward Voltage (Vf) = 1.2V typical (1.5V max)
Forward Current (If) = 100mA continuous (1A peak)
Peak Wavelength = 940nm
Viewing Angle = ±20°
```

#### Transistor Selection (example: BC547)
```
Type: NPN transistor
Vce(sat) = 0.2V @ Ic=100mA
Ic(max) = 100mA
hFE = 200 (typical)
```

#### Base Resistor Calculation
```
Assuming:
- PA6 output voltage = 3.3V
- Desired LED current (Ic) = 50mA (safe value)
- hFE = 200
- Base current (Ib) = Ic / hFE = 50mA / 200 = 0.25mA

Base Resistor (Rb) = (Vout - Vbe) / Ib
                   = (3.3V - 0.7V) / 0.25mA
                   = 2.6V / 0.25mA
                   = 10.4 kΩ

Use: 10 kΩ standard value (provides Ib = 0.26mA)
```

#### LED Current Limiting Resistor Calculation
```
Assumptions:
- Supply Voltage = 5V (from USB)
- LED Forward Voltage (Vf) = 1.2V
- Transistor Vce(sat) = 0.2V
- Desired LED current (If) = 50mA

LED Resistor (Rled) = (Vsupply - Vf - Vce(sat)) / If
                     = (5V - 1.2V - 0.2V) / 0.05A
                     = 3.6V / 0.05A
                     = 72Ω

Use: 82Ω standard value (provides If ≈ 44mA)
```

### Circuit Diagram
```
      +5V
       │
      [R_LED]  (82Ω)
       │
       ├─── IR LED (Cathode ↑, Anode ↓)
       │
       │
      [C] Collector
       │
PA6 ──[R_B]──[B] BC547 NPN
      (10kΩ) [E] Emitter
              │
             GND
```

### Power Calculations
```
LED Power = If × Vf = 50mA × 1.2V = 60mW
Resistor Power = If² × R = (0.05)² × 82 = 0.205W
Transistor Power = If × Vce(sat) = 50mA × 0.2V = 10mW

Use resistor with minimum 0.25W rating (1/4W)
```

---

## 5. Report Requirements Checklist

### Images to Include

#### 1. Oscilloscope Screenshot: 38kHz Carrier
- [ ] Clear waveform showing 38kHz carrier
- [ ] Cursor measurements visible
- [ ] Frequency and period labeled
- [ ] Duty cycle measurement visible
- [ ] Grid visible for reference

#### 2. Oscilloscope Screenshot: Duty Cycle Verification
- [ ] Zoomed view of 2-3 carrier periods
- [ ] Cursors measuring Ton and Toff
- [ ] Calculation shown: Duty Cycle = (Ton/T) × 100%
- [ ] Verification: ~25% duty cycle

#### 3. Oscilloscope Screenshot: RC5 Data Bits
- [ ] Complete frame visible (~25ms)
- [ ] Individual bits identified and labeled
- [ ] Start bits (S1, S2) marked
- [ ] Toggle bit marked
- [ ] Address bits (5 bits) decoded
- [ ] Command bits (6 bits) decoded
- [ ] Bit period measured (889 µs)

#### 4. Circuit Schematic
- [ ] Complete circuit from PA6 to IR LED
- [ ] All component values labeled
- [ ] Transistor pinout shown
- [ ] Power supply connections
- [ ] Ground connections

#### 5. Calculation Worksheet
- [ ] Base resistor calculation
- [ ] LED current limiting resistor calculation
- [ ] Power dissipation calculations
- [ ] Safety margin considerations
- [ ] Datasheet references

### Verification Tests

#### Test 1: Carrier Frequency
```
Measured Frequency: _______ kHz
Expected: 38 kHz
Error: _______ %
Status: PASS / FAIL (tolerance: ±5%)
```

#### Test 2: Duty Cycle
```
Measured Duty Cycle: _______ %
Expected: 25%
Error: _______ %
Status: PASS / FAIL (tolerance: ±10%)
```

#### Test 3: Bit Timing
```
Measured Bit Period: _______ µs
Expected: 889 µs
Error: _______ %
Status: PASS / FAIL (tolerance: ±5%)
```

#### Test 4: LED Blink on Button Press
```
Button Pressed: YES / NO
LED Blinks: YES / NO
IR Transmission Detected: YES / NO
Status: PASS / FAIL
```

#### Test 5: Frame Decoding
```
Start Bits (should be '11'): _______
Toggle Bit: _______
Address (5 bits): _______  (Decimal: _______)
Command (6 bits): _______  (Decimal: _______)

Expected Command: 12 (Volume Up)
Status: PASS / FAIL
```

---

## Tips for Better Measurements

### Oscilloscope Configuration
1. **Use AC coupling** when measuring only the carrier to remove DC offset
2. **Use DC coupling** when measuring data envelope to see actual logic levels
3. **Enable peak detection** mode for carrier measurements
4. **Use persistence** mode to capture complete frame
5. **Save screenshots** with cursors and measurements visible

### Trigger Settings
1. For carrier: Trigger on rising edge, level ~1.5V
2. For data: Trigger on rising edge at start of frame
3. Use **single shot** mode to capture complete transmission

### Common Issues
- **Noisy signal**: Check probe ground connection, use shorter ground lead
- **No trigger**: Adjust trigger level, check signal amplitude
- **Distorted waveform**: Check probe compensation (use scope calibration output)
- **Carrier not visible**: Increase sample rate (minimum 1 MSa/s)

---

## Example Report Structure

### Section 1: Introduction
- Project goals
- RC5 protocol overview
- Hardware platform

### Section 2: Hardware Design
- Circuit schematic
- Component selection justification
- Calculations with datasheet references

### Section 3: Measurements
#### 3.1 Carrier Wave (38kHz)
- Oscilloscope screenshot
- Measurements and calculations
- Verification against specifications

#### 3.2 Duty Cycle (25%)
- Zoomed oscilloscope screenshot
- Cursor measurements
- Calculation verification

#### 3.3 RC5 Data Frame
- Complete frame capture
- Bit identification
- Frame decoding
- Timing verification

### Section 4: Testing & Verification
- Button press demonstration
- LED indication
- Complete system operation

### Section 5: Conclusions
- Verification results summary
- Deviations from expected values
- Possible improvements

### Section 6: References
- Datasheets
- Application notes (AN4834)
- RC5 protocol specification

---

## Additional Resources

### Useful Links
- RC5 Protocol: https://www.sbprojects.net/knowledge/ir/rc5.php
- STM32L432KC Datasheet: https://www.st.com/resource/en/datasheet/stm32l432kc.pdf
- AN4834 (IR Remote Control): https://www.st.com/resource/en/application_note/an4834-stmicroelectronics.pdf

### Datasheet References
- IR LED (TSAL6200): Vishay datasheet
- Transistor (BC547): Fairchild/ON Semiconductor datasheet
- STM32L432KC: STMicroelectronics datasheet

---

*Document created for educational purposes - VIVES Hogeschool*
