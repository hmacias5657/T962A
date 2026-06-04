# Adaptive PID Reflow Oven Controller

AI-enhanced adaptive PID reflow oven temperature controller for the T962A oven, built on ESP32 WROOM with dual-core FreeRTOS architecture. Supports reflow soldering and baking/drying profiles. **BEWARE, NOT TESTED ON HARDWARE YET**

## Features

- **Dual-core FreeRTOS**: Core 0 handles thermodynamics (PID, Bresenham, ADC), Core 1 handles UI (display, buttons, buzzer)
- **Bresenham power distribution**: 256 half-cycle AC power modulation via zero-cross SSR for heater and cooling fan
- **Multi-zone PID**: 5 independent PID gain sets per recipe (Preheat/Soak/ReflowRamp/ReflowPeak/Cooldown) for both heater and cooling fan, saved per-zone in NVS and loaded on zone transitions
- **Feedforward control**: Target-ramp-rate feedforward uses measured plant heating/cooling rates to pre-emptively apply heater/cooling power — PID only corrects residual error
- **Plant model calibration**: Dedicated calibration cycle measures per-zone heating rates (°C/s at 100% output), natural cooling rates, and thermal deadtime; stored globally in NVS and used by feedforward on all subsequent runs
- **EMA temperature filtering**: 1-pole exponential moving average (α=0.15) removes ~85% of ADC noise from each thermocouple reading before it reaches the PID derivative term; fault detection reads raw value
- **AI supervisory gain tuner**: Spatial thermal gradient damping with learning heuristic for both heating and cooling; stage tuning natively embedded in per-zone gains
- **Dual K-type thermocouples**: Synced 12-bit ADS1015 ADC reading via I2C at zero-cross for noise immunity and linearity; per-sensor calibration offset adjustment
- **128x64 KS0108 GLCD**: Original T962A parallel display driven via U8g2; real-time target vs. actual temperature plot with firmware version and line frequency on splash screen
- **NVS-persistent recipes and PID gains**: Up to 10 user-created profiles stored in ESP32 NVS; per-zone heater and cooling PID coefficients saved after each cycle
- **Baking/drying profile**: User-selectable temperature (80–150°C) and duration (1–300 min) for baking solder paste, drying PCBs, or preheating
- **Configurable settings**: Adjustable max bake duration (1–300 minutes) and °C/°F unit toggle, saved to NVS
- **Calibration menu**: Per-sensor thermocouple offset adjustment (±10°C, 0.5°C steps), per-zone heater/cooling PID gain viewer, and **Calibration Run** — dry cycle that measures plant dynamics (heating/cooling rates, deadtime) and auto-tunes recipe times including cooldown (peak→120°C) with 1.2x safety margin
- **8-minute live plot**: Real-time target vs. actual temperature plot with 0–280°C Y-axis and 0–8 minute X-axis, adapts to slow oven thermal gradients
- **Line frequency auto-calibration**: Measures AC mains frequency at boot by averaging zero-cross ISR timestamps; PID time step and Bresenham window adapt dynamically for 50/60 Hz regions
- **System cooling fan**: Temperature-proportional PWM via MOSFET (40°C=0% → 80°C=100%) keeps controller electronics cool
- **Oven cooling fan**: Second Bresenham zero-cross SSR channel for proportional cooling during cooldown and overshoot suppression
- **Profile creation wizard**: Create new reflow profiles via the menu — adjust preheat/soak/peak temps, ramp/soak/reflow/hold times
- **Safety systems**: Over-temp cutoff (280°C), sensor fault detection, thermal gradient monitoring, dedicated hardware emergency stop
- **Audible diagnostics**: Buzzer patterns for phase transitions, cycle completion, and error codes
- **T962A original keypad**: F1/F2/F3/F4 directional keys + S key (SELECT/START) reused; dedicated HW E-STOP added

## Hardware Requirements

| Component | Specification |
|---|---|
| MCU | ESP32 WROOM (ESP32 Dev Module) |
| Oven | T962A reflow oven (or similar converted toaster oven) |
| Thermocouples | 2x K-type with analog amplifier ICs (AD8495) |
| External ADC | ADS1015 12-bit 4-channel I2C ADC for linearity |
| SSR (heater) | Zero-crossing solid-state relay for heating elements |
| SSR (cooling fan) | Zero-crossing solid-state relay for oven cooling fan |
| MOSFET | N-channel logic-level for system cooling fan PWM |
| Display | 128x64 monochrome KS0108 GLCD, 8-bit parallel 6800 mode (original T962A) |
| Buzzer | 5V active/passive buzzer |
| Buttons | T962A original 5-key membrane (F1=UP, F2=DOWN, F3=LEFT, F4=RIGHT, S=SELECT/START) + dedicated hardware E-STOP |
| Power | 5V DC for ESP32 and logic; AC mains for oven heaters via SSR |

### Pin Mapping

| GPIO | Signal | T962A Key |
|------|--------|------------|
| 4 | Zero-cross interrupt input (RISING) | — |
| 16 | Heater SSR gate output (Bresenham) | — |
| 17 | Oven cooling fan SSR gate (Bresenham) | — |
| 18 | Buzzer output | — |
| 36 | F1 / UP (navigate, increment) | F1 |
| 37 | F2 / DOWN (navigate, decrement) | F2 |
| 38 | F3 / LEFT (decrement in edit modes) | F3 |
| 39 | F4 / RIGHT (increment in edit modes) | F4 |
| 34 | S / SELECT (short press) / START (hold 1s) | S |
| 35 | Dedicated E-STOP (hardware, active-low) | — |
| 0 | System cooling fan PWM (MOSFET, 10k pull-up) | — |
| 22 | I2C SDA (ADS1015) | — |
| 23 | I2C SCL (ADS1015) | — |
| 25 | LCD D0 (data bit 0) | — |
| 26 | LCD D1 (data bit 1) | — |
| 27 | LCD D2 (data bit 2) | — |
| 32 | LCD D3 (data bit 3) | — |
| 33 | LCD D4 (data bit 4) | — |
| 19 | LCD D5 (data bit 5) | — |
| 21 | LCD D6 (data bit 6) | — |
| 5 | LCD D7 (data bit 7) | — |
| 14 | LCD RS (Register Select / A0) | — |
| 2 | LCD E (Enable strobe) | — |
| 15 | LCD CS1 (Chip select – left half) | — |
| 13 | LCD CS2 (Chip select – right half) | — |

### ADS1015 Channels

| Channel | Signal |
|---------|--------|
| A0 | Thermocouple 1 (via AD8495) |
| A1 | Thermocouple 2 (via AD8495) |
| A2 | Reserved for future board NTC temperature measurement |
| A3 | Reserved |

## Build & Flash

```bash
# Install PlatformIO (if not already)
pip3 install platformio

# Build
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Project Structure

```
├── Adaptive_Considerations.md # Design reference for adaptive PID, feedforward, ESP-IDF porting
├── platformio.ini             # PlatformIO build config (ESP32, U8g2, ADS1X15)
├── rename_firmware.py         # Post-build script: firmware.bin → firmware_v<VERSION>.bin
src/
├── main.cpp                   # Entry point, FreeRTOS dual-core task setup
├── Config.h                   # Pin assignments, constants, limits, firmware version
├── SharedData.h               # ReflowRecipe, ThermalTelemetry, state enums
├── BresenhamPID.cpp/.h        # Zero-cross ISR, dual Bresenham channels, PID + feedforward
├── TemperatureReader.cpp/.h   # ADS1015 I2C reading + EMA filter + calibration offsets
├── ProfileEngine.cpp/.h       # 5-stage thermal profile interpolation + target ramp rate
├── AITuner.cpp/.h             # Gain scheduler with spatial damping + learning for heater and cooling
├── DisplayRenderer.cpp/.h     # U8g2 KS0108 GLCD: menus, live plot, bake, settings, calibration
├── ButtonDebouncer.cpp/.h     # 50ms non-blocking debounce for 5 buttons
└── Buzzer.cpp/.h              # Audible pattern generator (phase, complete, error)
```

## License

MIT
