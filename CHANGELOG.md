# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.9.0] — 2026-05-31

### Added
- **Line frequency auto-calibration**: At startup, averages 40 zero-cross ISR edge timestamps using `esp_timer_get_time()` to measure the actual AC line half-cycle period. Line frequency is calculated as `1e6 / (2 * halfPeriodUs)`, validated against the 45–66 Hz range, and persisted to NVS under key `lineFreq`. If the measured frequency differs from the saved value by more than 0.1 Hz, it is updated in NVS.
- **Dynamic Bresenham window and PID time step**: `BRESENHAM_WINDOW_MS` and `PID_TIME_STEP_S` are no longer compile-time constants. `BresenhamPID::getWindowMs()` and `getTimeStepS()` return values computed from the measured half-cycle period, making the PID integrator and derivative terms accurate for the actual line frequency (50 Hz vs 60 Hz regions).
- **Splash screen frequency display**: Boot splash now shows measured line frequency (e.g. "Line: 60.0 Hz") alongside the firmware version.
- `BresenhamPID`: `calibrateLineFrequency()` method, calibration fields, `setMeasuredFrequency()`, `getMeasuredFrequency()`, `getMeasuredHalfPeriodUs()`, `getWindowMs()`, `getTimeStepS()`.

### Changed
- **Config.h**: Removed `AC_FREQ_HZ`, `BRESENHAM_WINDOW_MS`, `PID_TIME_STEP_S`, `ZC_HALF_CYCLE_US_DEFAULT` compile-time macros. Added `DEFAULT_FREQ_HZ` (60.0) fallback.
- **BresenhamPID.cpp**: `begin()` runs `calibrateLineFrequency()` after ISR attach; ISR records `esp_timer_get_time()` on first ZC edge and counts to 40; `runPIDLoop()` / `runCoolingPID()` use `getTimeStepS()` dynamically.
- **main.cpp**: Control loop uses `bresenhamPID.getWindowMs()` for `vTaskDelayUntil`; frequency is saved/compared to NVS after calibration; `display.setMeasuredFrequency()` called before splash.
- `DisplayRenderer`: Added `_measuredFreq` member, `setMeasuredFrequency()` setter, splash shows "Line: X.X Hz".

## [1.8.0] — 2026-05-31

### Changed
- **Display driver**: Replaced SSD1306 OLED (SPI) with original T962A KS0108 128x64 GLCD (8-bit parallel 6800 mode). Pin mapping updated to use 8 data lines (D0–D7), Enable strobe (E), Register Select (RS), and dual chip selects (CS1/CS2).
- **Config.h**: Complete pin reassignment: ZC→GPIO4, SSR→GPIO16, cooling fan SSR→GPIO17, buzzer→GPIO18, F1–F4→GPIO36–39, S→GPIO34, E-STOP→GPIO35, system fan→GPIO0, I2C→GPIO22/23, LCD data/control as per T962A original pinout.
- `DisplayRenderer`: Constructor type changed from `U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI` to `U8G2_KS0108_128X64_1` with 14-pin parallel wiring; `setContrast()` removed (KS0108 has no contrast register).
- `TemperatureReader`: Replaced `ADS1015_GAIN` macro reference with `GAIN_ONE` directly.
- LEDC constants changed to plain integers for Arduino framework compatibility.

## [1.7.0] — 2026-05-31

### Added
- **8-minute display window**: `PLOT_DURATION_S` 300→480, `TEMP_RANGE_MAX` 300→280; X-axis ticks at 0/2/4/6/8 min; Y-axis labels (280/140/0); `drawAxes()` now called in `renderLivePlot()`
- **Extended default recipe times**: Preheat ramp 90→120s, soak 90→120s, reflow 40→60s, peak hold 20→60s (~480s total window)
- **Extended profile creation limits**: Ramp/soak max 180→300s, reflow max 120→180s, hold max 60→120s
- **Calibration Run**: New 4th option in Calibration menu — runs a full dry cycle measuring actual time to reach preheat/soak/peak temps, then updates recipe times with 1.2x safety margin and saves to NVS
- New states: `STATE_CAL_RUNNING`, `STATE_CAL_COMPLETE`; phases: Preheat→Soak→Reflow→Hold(10s)→Cooling
- `Config.h`: `CAL_RUN_MARGIN` (1.2f), `CAL_PEAK_HOLD_S` (10), `FIRMWARE_VERSION` 1.7.0
- `DisplayRenderer`: `MENU_CAL_RUN_CONFIRM`, `MENU_CAL_RUNNING`, `MENU_CAL_COMPLETE`; `renderCalRunConfirm()`, `renderCalRunning()`, `renderCalComplete()`

### Changed
- Live plot screen now draws proper axes (horizontal/vertical lines, Y-axis labels)
- Calibration menu extended to 4 items (added "Cal Run" option)
- `rename_firmware.py`: version bumped to 1.7.0

## [1.6.0] — 2026-05-31

### Changed
- **Button mapping**: Replaced discrete START/E-STOP/UP/DOWN/SELECT with T962A original 5-key membrane + dedicated HW E-STOP
  - F1 = UP (navigate/increment)
  - F2 = DOWN (navigate/decrement)
  - F3 = LEFT (decrement in edit modes)
  - F4 = RIGHT (increment in edit modes)
  - S = SELECT (short press) / START (hold 1s)
  - Dedicated E-STOP on GPIO 19 (separate from T962A keypad)

### Added
- S key long-press detection (1s hold) for START actions (begin cycle, save settings, save calibration)
- LEFT (F3) / RIGHT (F4) directional keys for value adjustment in all edit contexts
- `isSShortPress()` / `isSLongPress()` helpers in `main.cpp` for dual-role S key handling
- Button reference aliases (`btnUp`, `btnDown`, `btnLeft`, `btnRight`, `btnSelect`, `btnStart`) mapped to physical T962A keys in `ButtonDebouncer.h`
- E-STOP on GPIO 19 as dedicated hardware pin; serves as emergency stop during active cycles and cancel/back in menu contexts

### Removed
- `PIN_BTN_START` (GPIO 12), `PIN_BTN_SELECT` (GPIO 33 standalone), `PIN_BTN_UP`/`PIN_BTN_DOWN` as separate functional pins — replaced by T962A membrane pinout

## [1.5.0] — 2026-05-30

### Added
- System cooling fan: GPIO 4 PWM via MOSFET, temperature-proportional speed (40°C=0% to 80°C=100%) for electronics cooling
- Oven cooling fan SSR: GPIO 15, second zero-cross Bresenham channel for proportional cooling during cooldown and overshoot suppression
- Dual-channel ISR: handles heater and cooling fan independently in same ZC interrupt (GPIO 25)
- Per-zone cooling PID gains (Kp, Ki, Kd) stored in NVS as `r{idx}_z{zone}_ckp/cki/ckd`, loaded/saved on zone transitions
- AITuner cooling inference: `runCoolingInference()` with spatial delta boost and learning heuristic; `setCoolingBaseGains()`
- Calibration menu: TC1/TC2 offset adjustment (±10°C, 0.5°C steps) plus per-zone heater and cooling PID gain viewer
- NVS persistence for calibration offsets (`tc1Off`, `tc2Off`) and per-zone cooling gains
- Over-temp limit raised to 280°C; profile preheat temperature minimum raised to 60°C

### Changed
- `BresenhamPID`: dual-accumulator ISR for heater + cooling fan; `runCoolingPID()`, `setCoolingGains()`, cooling gain getters; `emergencyStop()` zeros both channels
- `TemperatureReader`: added `_tc1Offset`, `_tc2Offset` members and `setCalibrationOffset()`/`getCalibrationOffset()` methods; offsets applied in `readSensors()`
- `AITuner`: added cooling-specific gain base and `runCoolingInference()` with spatial boost
- `DisplayRenderer`: added `MENU_CALIBRATION` and `MENU_CALIBRATION_GAINS` states; `renderCalibration()` and `renderZoneGains()` methods; `renderRecipeSelect` now shows "Calibration" option
- `SharedData.h`: `ThermalTelemetry` includes `coolingOutput` field
- `main.cpp`: recipeNavigation expanded to include Calibration option; cooling zone gains saved on transitions and during cooldown; system fan PWM via `ledcAttach`; calibration FSM with offset edit mode and zone gain viewer
- `Config.h`: new defines for cooling pins, fan constants, calibration constants, zone defaults; OVER_TEMP_LIMIT 280.0f; FIRMWARE_VERSION 1.5.0
- `platformio.ini`: No library changes needed

## [1.4.0] — 2026-05-30

### Added
- °C/°F unit toggle in Settings menu: 2-row UI (Max Bake / Units), SELECT toggles between rows, UP/DOWN adjusts active setting, START saves both to NVS
- NVS persistence for unit preference under key `useF` (0=°C, 1=°F); loaded on boot and applied to all temperature display
- `DisplayRenderer::toDisplayTemp()` helper converts °C → °F when `_useFahrenheit` flag is set; used in live plot, bake running, bake setup, recipe select, and profile creation wizard
- `DisplayRenderer::setUseFahrenheit(bool)` to update the flag at runtime after settings save
- Firmware version (`FIRMWARE_VERSION "1.4.0"`) displayed on splash screen and serial boot message
- Post-build Python script (`rename_firmware.py`) copies `firmware.bin` → `firmware_v1.4.0.bin` so the version is embedded in the filename

### Changed
- `DisplayRenderer.h`: Added `_useFahrenheit` member, `setUseFahrenheit()`, `toDisplayTemp()`; updated `renderProfileCreate` signature with `bool isTemperature` parameter; updated `renderSettings` signature with `int selectedRow` and `bool useFahrenheit`
- `DisplayRenderer.cpp`: All temperature outputs now call `toDisplayTemp()` and show `F`/`C` suffix; `renderSettings` draws two rows with cursor; splash screen shows `AI-PID v1.4.0`
- `main.cpp`: Added `useFahrenheit` global, `settingsSelectedRow` for 2-row navigation; `initNvs` loads `useF` from NVS; settings handler supports row toggle via SELECT and per-row UP/DOWN adjustment; unit saved to NVS on START; `display.setUseFahrenheit()` called on boot and after settings save; serial boot message uses `FIRMWARE_VERSION`
- `Config.h`: Added `FIRMWARE_VERSION` define (`"1.4.0"`)
- `platformio.ini`: Added `extra_scripts = post:rename_firmware.py` for versioned firmware binary output
- Internal control loop, PID, safety limits, and profile engine remain in °C — only the display layer converts

## [1.3.0] — 2026-05-30

### Added
- Multi-zone PID: 5 independent PID gain sets per recipe (Preheat/Soak/ReflowRamp/ReflowPeak/Cooldown) stored in NVS as `r{idx}_z{zone}_kp/ki/kd`
- Zone transition logic in Core 0 control loop: saves current BresenhamPID gains to previous zone, loads saved gains for new zone at each stage boundary
- Baking/drying profile mode: `STATE_BAKE_RUNNING` / `STATE_BAKE_COMPLETE` with user-selectable temperature (80–150°C) and duration (1–300 min)
- Bake setup UI (`MENU_BAKE_SETUP`): UP/DOWN adjusts active field, SELECT toggles between temp and duration, START begins bake cycle
- Settings menu (`MENU_SETTINGS`): adjust max bake duration, saved to NVS under key `maxBakeMin`
- "Baking Profile" and "Settings" entries in recipe select menu alongside "Create New..."
- Default per-zone gains seeded from AITuner stage multipliers applied to base (Kp=2.0, Ki=0.05, Kd=1.2)

### Changed
- `Config.h`: Added `BAKE_TEMP_MIN/MAX`, `BAKE_DURATION_DEFAULT/MIN/MAX_DEFAULT`, `ZONE_KP/KI/KD_DEFAULT` constants; added `NUM_ZONES` and baking constants
- `SharedData.h`: Added `RecipeType` enum (`RECIPE_REFLOW`, `RECIPE_BAKE`), `NUM_ZONES` constant, `RecipeType type` and `bakeTemp`/`bakeDuration` fields to `ReflowRecipe`; added `STATE_BAKE_RUNNING`, `STATE_BAKE_COMPLETE` to `SystemState`; added `PidGains` struct
- `ProfileEngine.cpp/.h`: `calculateTargetTemp` and `getCurrentStage` handle `RECIPE_BAKE` type (returns constant bake temp, stage always `STAGE_SOAK`)
- `AITuner.cpp/.h`: Added `setStageTuningEnabled(bool)`; stage tuning disabled by default (per-zone gains already embed zone-specific tuning); removed `isBakeMode` parameter from `runInference`
- `DisplayRenderer.h/.cpp`: Added `MENU_BAKE_SETUP`, `MENU_BAKE_RUNNING`, `MENU_SETTINGS` menu states; `renderBakeSetup()`, `renderBakeRunning()`, `renderSettings()` methods; `renderRecipeSelect()` now shows "Baking Profile" and "Settings" options
- `main.cpp`: `recipePidGains` is now `[RECIPE_COUNT_MAX][NUM_ZONES]`; per-zone NVS save/load via `savePidGainsForRecipeZone()`; zone transition detection saves current gains / loads stored gains; baking state machine with timer expiry; settings menu saves `maxBakeMin` to NVS; recipe select navigates 3 extra options (create, bake, settings)

### Removed
- Single `PidGains` per recipe: replaced by per-zone array; old NVS keys `r{idx}_kp/ki/kd` are no longer used (zone keys loaded instead)

## [1.2.0] — 2026-05-30

### Added
- NVS persistence for reflow recipes using ESP32 Preferences library; recipes survive reboots
- Profile creation UI: "Create New..." option in recipe select menu with 7-parameter editor (preheat/soak/peak temps and ramp/soak/reflow/hold times)
- PID gain persistence per recipe: adaptive Kp/Ki/Kd coefficients saved to NVS at the end of each cycle and reloaded when the recipe is next selected
- `PidGains` struct in `SharedData.h`; `recipePidGains[]` array and NVS save/load functions in `main.cpp`
- `Preferences` library dependency (built-in ESP32 Arduino core)

### Changed
- `Config.h`: `RECIPE_COUNT` replaced with `RECIPE_COUNT_DEFAULT` (3) and `RECIPE_COUNT_MAX` (10); added `NVS_NAMESPACE`
- `DisplayRenderer.h/.cpp`: Added `MENU_PROFILE_CREATE` state, `renderProfileCreate()` method, and "Create New..." entry in `renderRecipeSelect()`
- `main.cpp`: Dynamic recipe navigation wraps from 0 to `storedRecipeCount` (including the create option); cycle completion saves PID gains to NVS for the active recipe

## [1.1.0] — 2026-05-30

### Changed
- Replaced ESP32 internal 12-bit ADC with ADS1015 external 12-bit I2C ADC for improved linearity and noise immunity
- `TemperatureReader`: `analogRead()` → `Adafruit_ADS1015.readADC_SingleEnded()` on I2C channels A0/A1
- `Config.h`: Removed `PIN_TC1_ADC` (GPIO 34), `PIN_TC2_ADC` (GPIO 35); added `PIN_I2C_SDA` (GPIO 21), `PIN_I2C_SCL` (GPIO 22), `ADS1015_ADDR`, `ADS1015_GAIN`

### Added
- `platformio.ini`: `adafruit/Adafruit ADS1X15@^2.4.0` library dependency
- Temperature conversion uses `computeVolts()` for gain-independent scaling (10mV/°C from AD595)

## [1.0.0] — 2026-05-30

### Added
- Initial release of the Adaptive PID Reflow Oven Controller for T962A
- Dual-core FreeRTOS architecture: Core 0 (control) / Core 1 (UI)
- Bresenham power distribution over 256 half-cycles via zero-cross SSR
- 12-bit ADC sampling synchronized to zero-crossing interrupt
- Dual K-type thermocouple reading with spatial delta monitoring
- 5-stage thermal profile interpolation engine (Preheat, Soak, Reflow Ramp, Reflow Peak, Cooldown)
- AI supervisory gain scheduler with stage-based PID tuning and spatial damping
- 3 pre-programmed reflow recipes: Sn63/Pb37 (220°C), SAC305 (245°C), Bi58/Sn42 (170°C)
- 128x64 OLED display (SSD1306, U8g2, HW SPI) with live temperature-vs-time plot
- 5-button menu system: START, E-STOP, UP, DOWN, SELECT
- Non-blocking button debouncing (50ms)
- Buzzer alert system: 4 patterns for phase transitions, cycle complete, errors, E-stop
- Safety systems: over-temp cutoff (>260°C), sensor fault (<5°C), spatial gradient (>45°C)
- Emergency stop with instant SSR shutdown and continuous alarm
- PlatformIO build configuration for ESP32 WROOM (240 MHz, QIO flash)
