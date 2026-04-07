# Lasertag-Game-opdracht-1

## Fase 2: RC5 IR Receiver

### Project overview
Dit project decodeert inkomende RC5 IR-signalen op een NUCLEO-L432KC.
De decoder gebruikt TIM2 input capture om pulsbreedtes te meten en zet die
om naar Manchester halfbits. Daarna wordt het 14-bit RC5 frame uitgepakt naar:
- startbits
- toggle bit
- address (5 bit)
- command (6/7 bit)

## Hardware

### Microcontroller
- Board: NUCLEO-L432KC
- MCU: STM32L432KC (Cortex-M4)

### Aansluitingen
| Pin | Functie | Omschrijving |
|---|---|---|
| PA0 | TIM2_CH1 (AF1) | IR ontvangersignaal (demodulated digital output) |
| PA2 | USART2_TX (AF7) | Seriele debug output (115200 baud) |
| PA15 | USART2_RX (AF7) | Seriele input (optioneel) |

Tip: gebruik een 38 kHz IR receiver module (bv. VS1838B/TSOP type) waarvan de
digitale uitgang naar PA0 gaat.

## Software architectuur

### Timerconcept (TIM2)
TIM2 is geconfigureerd in input-capture slave reset mode:
- Falling edge op CH1 reset de teller en markeert start van een nieuw segment.
- CH1 capture geeft de totale periode tussen twee relevante randen.
- CH2 capture geeft de low-tijd binnen die periode.
- In de callback wordt high-tijd berekend als: high = period - low.

Met deze twee tijden bepaalt de code of een segment 1 of 2 RC5-halfbits lang is.

### RC5 decodeflow
1. ISR leest periodUs en lowUs uit TIM2 captures.
2. Segmenten worden gekwantiseerd naar 1 of 2 halfbits.
3. Halfbits worden in een buffer geplaatst als logische levels (0 of 1).
4. Decoder zoekt een geldig 14-bit Manchester frame.
5. Bij geldige startbits wordt frame uitgepakt en geprint via UART.

## RC5 protocol (samenvatting)
- Frame lengte: 14 bits
- Volgorde: S1, S2/Field, Toggle, Address[4:0], Command[5:0]
- Codering: Manchester
- Bitduur: ongeveer 1.778 ms (2 halfbits)
- Halfbits: ongeveer 889 us

In de implementatie worden toleranties gebruikt (vensters) om clock-jitter en
sensorvariatie op te vangen.

## Build en run
1. Open het project in Keil uVision of STM32CubeIDE.
2. Build en flash naar de NUCLEO-L432KC.
3. Open serial terminal op 115200-8-N-1.
4. Richt een RC5 afstandsbediening naar de ontvanger.
5. Controleer logs zoals:

```text
[RC5] Raw: 0x3XXX | Addr: 0xYY | Cmd: 0xZZ | Toggle: N
```

## Mondelinge verdediging: korte praatlijn

Gebruik dit als kapstok tijdens je uitleg:
1. Meetprincipe: "Ik meet low en high duur per pulssegment met input capture."
2. Kwantisatie: "Die tijden map ik naar 1 of 2 halfbits met tolerantievensters."
3. Manchester: "Per bit verwacht ik ofwel 10 of 01; anders verwerp ik."
4. Validatie: "Ik controleer de startbits voor een geldig RC5 frame."
5. Parsing: "Dan splits ik raw frame in toggle, address en command."
6. Debug: "Resultaat stuur ik via UART zodat ik live kan verifiëren."

## Troubleshooting

### Geen decode output
- Check dat receiver output echt op PA0 zit.
- Controleer of de afstandsbediening RC5 gebruikt.
- Controleer UART instellingen (115200-8-N-1).

### Veel foutieve/instabiele decodes
- Vermijd direct zonlicht op de IR receiver.
- Controleer voeding en ground van de receiver module.
- Pas timingvensters aan indien nodig voor jouw hardware.

### Wel interrupts, geen geldige frames
- Controleer Manchester interpretatie in de halfbitbuffer.
- Controleer startbit-validatie in de decoder.

## Belangrijke bestanden
- Opdracht 2/Core/Src/main.c: applicatie en RC5 decode logica
- Opdracht 2/Core/Src/stm32l4xx_it.c: interrupt handlers
- Opdracht 2/Core/Src/stm32l4xx_hal_msp.c: GPIO/NVIC MSP configuratie

## License
Educational use only
