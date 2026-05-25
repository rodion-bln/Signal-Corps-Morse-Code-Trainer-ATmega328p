# Signal Corps - Old School Morse Code Trainer

An embedded device for training and decoding Morse code, built around the ATmega328P-XMINI board and a vintage Soviet telegraph key.

## What it does

The system has two modes, switched via the SW2 button:

- **Trainer mode**: a random letter appears on the screen, the user transmits it through the Morse key, and the system checks whether it was correct
- **Decoder mode**: any incoming Morse signal is decoded into text and displayed on the LCD

The distinctive feature of this project is that decoding is done through **audio capture with a microphone**, not by digitally reading the key. The key triggers the buzzer, and the KY-037 microphone listens to the tone and measures the duration of each pulse. This makes the device behave like a real CW decoder and means it can interpret tones from any audio source, not just its own key.

## Repository structure

```
.
- pm_proiect.ino                # Main Arduino sketch
- Schematic_SignalCorps.png     # Electrical schematic (Autodesk Fusion)
- SignalCorpsDiagramDrawio.png  # Block diagram (draw.io)
- WokwiSignalCorps.jpeg         # Wokwi simulation screenshot
- atmega328p.lbr                # Custom Fusion library for ATmega328P-XMINI
- final1.jpg                    # Final assembly photo
- final2.jpg                    # Final assembly photo
- README.md
```

## Hardware components

| Component | Role |
|---|---|
| ATmega328P-XMINI | Main microcontroller |
| LCD 1602 I2C (0x27) | Displays current mode and score |
| OLED 0.96" SSD1306 (0x3C) | Graphical display, live oscilloscope |
| KY-037 | Sound sensor for tone capture |
| Passive piezo buzzer | Generates 2500 Hz tone |
| Vintage telegraph key | Primary input interface |
| 2x LEDs + 220Ω resistors | Correct/wrong feedback |
| Push-button | Trainer/Decoder mode selector |

## Wiring

```
PD2 -> Morse key (to GND)
PD3 -> Piezo buzzer (to GND)
PD4 -> Mode button (to GND)
PD5 -> Green LED through 220Ω resistor to GND
PD6 -> Red LED through 220Ω resistor to GND
A0  -> KY-037 AO (analog output)
A4  -> SDA (shared by LCD and OLED)
A5  -> SCL (shared by LCD and OLED)
```

Power supply at 5V comes via USB from the X-MINI board.

## Required libraries

Install through Library Manager in Arduino IDE:

- Wire.h (built-in)
- Adafruit_SSD1306
- Adafruit_GFX
- LiquidCrystal_I2C

## How to use

1. Connect the board to USB
2. Wait 3 seconds for automatic microphone calibration (stay quiet during this period)
3. The first letter to transmit appears on the OLED screen
4. Press the key: short press for dot, long press for dash
5. After you finish transmitting, the system verifies the answer with a short pause
6. Press the SW2 button to toggle between Trainer and Decoder mode

## Technical details

- Buzzer frequency: 2500 Hz, chosen to match the natural resonance of the piezo for maximum volume
- Microphone sampling: 16 readings in 4ms, covering about 10 cycles of the sound wave to reliably catch the peak
- Morse timing: dot under 250ms, dash between 250-1500ms, character pause 800ms
- Speed approximately 10 WPM, suitable for beginners
- I2C communication at 100 kHz for stability with low-cost OLEDs

## Implementation notes

The project went through a major design change during development. Initial versions used digital reading of the key for decoding, but the approach was replaced with audio capture through the microphone. This makes the project closer to a real CW decoder and makes proper use of the KY-037 sensor through the ADC instead of treating it as auxiliary.

Several optimizations were necessary to fit everything into the 2KB of RAM available on the ATmega328P:

- All strings replaced with fixed `char[]` arrays to avoid dynamic allocation
- Morse code table stored in PROGMEM (Flash memory) instead of RAM
- Microphone reading done in burst mode for reliable tone detection
- Hysteresis on detection (ON threshold higher than OFF threshold) to avoid oscillation at the threshold edge
- Continuous adaptation of the baseline to ambient noise levels

## Lab topics covered

The project integrates concepts from five course laboratories:

- GPIO for LEDs and buttons with INPUT_PULLUP
- UART for serial debugging at 9600 baud
- Timers and PWM for the buzzer through the tone() function
- ADC for reading the KY-037 microphone on pin A0
- I2C with a shared bus for both displays at different addresses

## Project for

Course: Microcontroller Programming, ACS UPB, 2026
