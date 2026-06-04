# AGENTS.md — Session Context

## Project: Adaptive PID Reflow Oven Controller

### Date
June 4, 2026

### Prompt Origin
The user provided a detailed markdown specification (`Adaptive PID reflow oven controller.md`) outlining an AI-enhanced adaptive PID reflow oven controller for the T962A oven. The specification evolved through iterative prompting covering hardware selection, control algorithms, dual-core architecture, UI design, and safety systems.

### Target Hardware
- **MCU**: ESP32 WROOM (ESP32 Dev Module, 240 MHz, dual-core Xtensa LX6)
- **Oven**: T962A reflow oven with original 128x64 display
- **Sensors**: 2x K-type thermocouples via analog amplifier ICs (AD595/LT1025) with cold-junction compensation
- **External ADC**: ADS1015 12-bit 4-channel I2C ADC (avoids ESP32 internal ADC linearity errors)
- **SSR**: Zero-crossing solid-state relay (heater + oven cooling fan)
- **MOSFET**: N-channel logic-level for system cooling fan PWM
- **Display**: 128x64 monochrome KS0108 GLCD (8-bit parallel 6800 mode, original T962A)
- **Buzzer**: Active/passive buzzer (GPIO 18)
- **Buttons**: T962A original 5-key membrane (F1=UP, F2=DOWN, F3=LEFT, F4=RIGHT, S=SELECT/START) + dedicated hardware E-STOP
- **E-STOP**: GPIO 35, active-low, separate from T962A keypad
- **ZC Interrupt**: GPIO 4, INPUT_PULLUP, RISING edge
- **S Key**: GPIO 34, short press = SELECT, hold 1s = START (dual-role handled in core1UITask)

### Design Decisions

| Decision | Rationale |
|----------|-----------|
| Dual-core FreeRTOS | Core 0 for control (PID/Bresenham/ADC), Core 1 for UI (display/buttons/buzzer) — prevents UI lag from affecting thermal regulation |
| Bresenham power distribution | 256 half-cycles evenly spread at zero-cross — avoids EMI/stress of high-frequency PWM while maintaining linearity |
| AI as supervisory gain scheduler | Not a neural network on-device (no TensorFlow Lite dependency); uses deterministic stage-and-delta rules that emulate adaptive behavior |
| U8g2 library | Widely available, supports KS0108 via 8-bit parallel with minimal flash footprint (~355KB used) |
| Queue + Mutex IPC | `xQueueOverwrite` ensures Core 1 always gets latest telemetry; mutex protects shared recipe/state from simultaneous access |
| xQueueOverwrite vs xQueueSend | Guarantees no stale data buildup if display rendering takes longer than one control window |
| Per-zone PID gains | 5 zones per recipe saved independently in NVS — zone gains converge over successive runs without cross-zone interference |
| ADS1015 external ADC | Replaces ESP32 internal ADC for linear 12-bit conversion; communicates via I2C with programmable gain |
| Preferences library | Built-in ESP32 NVS wrapper; used for recipe and config persistence |
| Stage tuning disabled by default | Per-zone gains already embed zone-specific tuning; AITuner only applies spatial damping and learning heuristic |
| Dual Bresenham channels | Heater + cooling fan share same ZC ISR with independent accumulators — proportional cooling without extra interrupts |
| Feedforward separate from AI | `computeFeedforward()` uses plant model (heating/cooling rates) for pre-emptive power; AI scheduler only handles spatial damping and learning on residual PID error |
| Plant model globally in NVS | Heating rate, cooling rate, deadtime stored as global NVS keys (`phr0-4`, `pcr0-4`, `pdt`) — not per-recipe — measured once by calibration, used by all subsequent runs |
| EMA filter in TemperatureReader | 1-pole α=0.15 on each TC reading before PID derivative; reset on cycle start; fault detection on raw value |
| System fan PWM on GPIO 4 | DC fan via MOSFET with temp-proportional speed keeps controller electronics cool |
| Calibration offsets per sensor | TC1/TC2 offsets stored in NVS and applied in readSensors() — compensates for amplifier/ADC tolerances without hardware changes |

### Architecture

```
Core 0 (Priority 3, 4KB stack)
  └─ Control Loop (~2.13s period)
       ├─ Wait for ADC trigger from ZC ISR
       ├─ Read 2x thermocouples via 12-bit ADC (with EMA filter)
       ├─ Compute target temp + ramp rate from ProfileEngine
       ├─ On zone transition: save current gains, load stored zone gains
       ├─ Compute feedforward power from plant model (heating/cooling rates)
       ├─ Run AITuner gain scheduling (spatial damping + learning)
       ├─ Execute PID → add feedforward → BresenhamOutput (0-256)
       ├─ Cooling fan control with cooling feedforward
       ├─ System fan PWM (temp-proportional)
       ├─ Safety checks (over-temp, sensor fault, spatial delta)
       └─ Push ThermalTelemetry to Queue

Core 1 (Priority 1, 8KB stack)
  └─ UI Loop (~30ms period)
       ├─ Splash screen on boot (version + line frequency)
       ├─ Menu state machine (MAIN / RECIPE_SELECT / RUNNING / ERROR / ESTOP
       │                    / PROFILE_CREATE / BAKE_SETUP / BAKE_RUNNING / SETTINGS
       │                    / CALIBRATION / CALIBRATION_GAINS)
       ├─ Button debouncing (50ms)
       ├─ Receive telemetry from Queue
       ├─ Render display (U8g2 KS0108 128x64)
       └─ Drive buzzer patterns

ISR (IRAM)
  └─ onZeroCrossing()
       ├─ Line frequency calibration (first 40 edges at boot)
       ├─ Bresenham accumulator → Heater SSR gate (GPIO 16)
       ├─ Bresenham accumulator → Cooling fan SSR gate (GPIO 17)
       └─ Cycle counter → ADC trigger every 256 cycles
```

### Key Files

| File | Lines | Purpose |
|------|-------|---------|
| `platformio.ini` | 20 | PlatformIO build config (ESP32, U8g2, Adafruit ADS1X15 deps) |
| `rename_firmware.py` | 13 | Post-build script: copies firmware.bin → firmware_v{VERSION}.bin |
| `src/Config.h` | 125 | All pin mappings, constants, limits, baking/zone/cooling/cal defaults, FIRMWARE_VERSION; T962A keypad pinout |
| `src/SharedData.h` | 69 | ReflowRecipe, PidGains, ThermalTelemetry (incl. coolingOutput), state and stage enums |
| `src/main.cpp` | 1378 | FreeRTOS entry, dual-core task orchestration, NVS, zone transitions, baking/calibration state machines, system fan, cooling control |
| `src/BresenhamPID.cpp` | 334 | ZC ISR, dual-channel Bresenham (heater + cooling), PID loop, feedforward, line frequency calibration, plant model measurement |
| `src/BresenhamPID.h` | 106 | BresenhamPID class interface with cooling channel, feedforward, plant model, and calibration |
| `src/TemperatureReader.cpp` | 56 | ADS1015 I2C reading, °C conversion, EMA filter, fault flags, calibration offsets |
| `src/TemperatureReader.h` | 38 | TemperatureReader class interface, TemperatureData struct, EMA filter state |
| `src/ProfileEngine.cpp` | 101 | 5-stage linear interpolation + target ramp rate + bake mode |
| `src/ProfileEngine.h` | 23 | ProfileEngine class interface |
| `src/AITuner.cpp` | 103 | Gain scheduler: heater + cooling spatial damping, learning heuristic |
| `src/AITuner.h` | 40 | AITuner class interface (heater + cooling gain scheduling) |
| `src/DisplayRenderer.cpp` | 551 | U8g2 screens: main menu, recipe select, live plot, error, e-stop, profile create, bake setup, bake running, settings, calibration, zone gains |
| `src/DisplayRenderer.h` | 64 | DisplayRenderer class interface, MenuState enum (11 states) |
| `src/ButtonDebouncer.cpp` | 58 | 50ms non-blocking debounce for 5 buttons |
| `src/ButtonDebouncer.h` | 45 | ButtonDebouncer class interface, ButtonState struct, T962A key aliases |
| `src/Buzzer.cpp` | 81 | 5 buzzer patterns with timing |
| `src/Buzzer.h` | 33 | Buzzer class interface, BuzzerPattern enum |

### Build Result (current)
```
RAM:   7.7%  (25264 / 327680 bytes)
Flash: 5.4%  (355121 / 6553600 bytes)
0 errors, 0 warnings
```

### Build Fixes Applied (history)
1. **`BresenhamPID.cpp`**: Lambda-to-function-pointer conversion issue. Replaced capturing lambda with a static trampoline function (`_isrZeroCrossing`) and instance pointer.
2. **`DisplayRenderer.cpp`**: Wrong U8G2 constructor — HW SPI variant takes 4 args (rotation, cs, dc, reset), not 6 (removed clk, mosi). Replaced with KS0108 14-pin constructor.
3. **`main.cpp`**: Typo — `core1UITaskHandle` → `core1TaskHandle`.
4. **`DisplayRenderer.cpp`**: Removed `setContrast()` call — KS0108 GLCD has no contrast register (SSD1306-only feature).
5. **`TemperatureReader.cpp`**: Replaced `ADS1015_GAIN` macro (compile error) with `GAIN_ONE` from Adafruit library enum.
6. **`Config.h`**: Removed `AC_FREQ_HZ`, `BRESENHAM_WINDOW_MS`, `PID_TIME_STEP_S`, `ZC_HALF_CYCLE_US_DEFAULT` — now computed dynamically from measured line frequency.

### Feature History

#### v1.0.0 — Initial Release
- Dual-core FreeRTOS, Bresenham power distribution, 12-bit ADC, dual TC, 5-stage profile, AI gain scheduling, 3 default recipes, 128x64 OLED, 5-button menu, buzzer, safety systems
- ADC sampling synced to ZC interrupt (GPIO 25, RISING)

#### v1.1.0 — ADS1015 External ADC
- Replaced ESP32 internal ADC with ADS1015 12-bit I2C ADC
- `TemperatureReader`: `analogRead()` → `Adafruit_ADS1015.readADC_SingleEnded()` via I2C
- `Config.h`: Removed `PIN_TC1_ADC` (GPIO 34), `PIN_TC2_ADC` (GPIO 35); added I2C pins `PIN_I2C_SDA` (GPIO 21), `PIN_I2C_SCL` (GPIO 22)
- `platformio.ini`: Added `adafruit/Adafruit ADS1X15@^2.4.0` dependency
- Temperature conversion uses `computeVolts()` for gain-independent scaling (10mV/°C from AD595)

#### v1.2.0 — NVM Profile & PID Persistence
- Recipes stored in ESP32 NVS via `Preferences` library; survives reboots
- Up to 10 recipes supported; 3 defaults shipped on first boot
- PID coefficients (Kp, Ki, Kd) saved per-recipe at cycle end and reloaded when recipe is selected
- New "Create New..." option in recipe select menu enters a 7-parameter editing wizard
- Parameters: preheat temp, soak temp, peak temp, ramp time, soak time, reflow time, hold time
- UP/DOWN adjust current parameter value by ±5; SELECT confirms and advances to next step
- Profile creation state machine in `main.cpp` (`MENU_PROFILE_CREATE`)

#### v1.3.0 — Multi-Zone PID, Baking Profile, Settings (current)
- 5 independent PID gain sets per recipe (Preheat/Soak/ReflowRamp/ReflowPeak/Cooldown) stored in NVS
- Zone transition logic saves current BresenhamPID gains to previous zone, loads stored gains for new zone
- Baking/drying profile mode: `STATE_BAKE_RUNNING` / `STATE_BAKE_COMPLETE`, user-selectable temp (80–150°C) and duration (1–300 min)
- Settings menu: adjust max bake duration, saved to NVS as `maxBakeMin`
- "Baking Profile" and "Settings" entries in recipe select menu
- AITuner stage tuning disabled by default (per-zone gains embed zone tuning); AITuner handles spatial damping and learning only
- Default per-zone gains seeded from AITuner stage multipliers applied to base (Kp=2.0, Ki=0.05, Kd=1.2)
- `ReflowRecipe` struct extended with `RecipeType type`, `bakeTemp`, `bakeDuration` fields
- `Config.h`: added baking constants (`BAKE_TEMP_MIN/MAX`, `BAKE_DURATION_*`) and zone default constants

#### v1.4.0 — °C/°F Unit Toggle, Firmware Versioning
- Settings menu expanded to 2 rows: Max Bake (row 0) and Units (row 1); SELECT toggles between rows, UP/DOWN adjusts active row, START saves both to NVS
- Unit preference stored in NVS under key `useF` (0=°C, 1=°F), loaded on boot
- `DisplayRenderer::toDisplayTemp()` converts °C → °F for all displayed temperatures (live plot, bake running, bake setup, recipe select, profile creation wizard)
- Internal control loop, PID, safety limits, and profile engine remain in °C — only the display layer converts
- `DisplayRenderer.h/.cpp`: added `_useFahrenheit`, `setUseFahrenheit()`, `toDisplayTemp()`; updated `renderSettings()` and `renderProfileCreate()` signatures
- Firmware version (`FIRMWARE_VERSION "1.4.0"`) displayed on splash screen and serial boot message
- `Config.h`: added `FIRMWARE_VERSION` define
- `platformio.ini`: added `extra_scripts = post:rename_firmware.py` for versioned firmware binary output
- Post-build Python script (`rename_firmware.py`) copies `firmware.bin` → `firmware_v1.4.0.bin`

#### v1.5.0 — System Fan, Oven Cooling Fan, Calibration (current)
- **System cooling fan**: GPIO 4 PWM via MOSFET, temp-proportional speed (40°C=0% → 80°C=100%), keeps controller electronics cool
- **Oven cooling fan SSR**: GPIO 15, second zero-cross Bresenham channel for proportional cooling during cooldown and overshoot suppression
- `BresenhamPID`: dual-channel ISR handles heater and cooling fan independently in same ZC interrupt; added `runCoolingPID()`, `setCoolingGains()`, cooling gain getters; `emergencyStop()` zeros both channels
- `TemperatureReader`: per-sensor calibration offsets (`_tc1Offset`, `_tc2Offset`) applied to raw readings; methods `setCalibrationOffset()`, `getCalibrationOffset()`
- `AITuner`: added `runCoolingInference()` with spatial delta boost (more cooling when gradient is high) and learning heuristic; `setCoolingBaseGains()`
- **Calibration menu**: new `MENU_CALIBRATION` / `MENU_CALIBRATION_GAINS` states in recipe select; TC1/TC2 offset adjustment (±10°C, 0.5°C steps); per-zone heater and cooling PID gain viewer (PRE/SOAK/RAMP/PEAK/COOL)
- NVS persistence for: `tc1Off`, `tc2Off` (floats), per-zone cooling gains (`r{idx}_z{zone}_ckp/cki/ckd`)
- Over-temp limit raised to 280°C; profile creation preheat min temp changed to 60°C
- Cooling gains stored per-recipe-per-zone, loaded on zone transitions and saved on exit; AITuner adapts spatially
- `Config.h`: added `PIN_COOLING_FAN_SSR`, `PIN_SYS_FAN_PWM`, cooling zone defaults, system fan constants, calibration constants, `OVER_TEMP_LIMIT` 280.0f, `FIRMWARE_VERSION` 1.5.0
- `SharedData.h`: added `coolingOutput` field to `ThermalTelemetry`

#### v1.6.0 — T962A Keypad Mapping
- **Button mapping**: Replaced discrete buttons with T962A original 5-key membrane (F1=UP, F2=DOWN, F3=LEFT, F4=RIGHT, S=SELECT/START) + dedicated HW E-STOP (GPIO 19)
- S key long-press detection (1s hold) for START actions
- `isSShortPress()` / `isSLongPress()` helpers for dual-role S key
- E-STOP serves as emergency stop during active cycles and cancel/back in menus

#### v1.7.0 — 8-Minute Display Window + Calibration Run
- **Display axes**: `PLOT_DURATION_S` 300→480 (8 min), `TEMP_RANGE_MAX` 300→280°C; X-axis ticks at 0/2/4/6/8 min; Y-axis labels (280/140/0); `drawAxes()` now called in `renderLivePlot()`
- **Extended recipe times**: Default recipes ramp 90→120s, soak 90→120s, reflow 40→60s, hold 20→60s (~480s total); profile creation max limits extended to 300/300/180/120s
- **Calibration Run**: New 4th option in Calibration menu ("Cal Run") — runs a full dry cycle measuring actual time to reach preheat/soak/peak temps, then updates recipe times with 1.2x safety margin and saves to NVS
- New states: `STATE_CAL_RUNNING`, `STATE_CAL_COMPLETE`; phases: Preheat→Soak→Reflow→Hold(10s)→Cooling
- `Config.h`: `CAL_RUN_MARGIN` (1.2f), `CAL_PEAK_HOLD_S` (10), `FIRMWARE_VERSION` 1.7.0
- `DisplayRenderer`: `MENU_CAL_RUN_CONFIRM`, `MENU_CAL_RUNNING`, `MENU_CAL_COMPLETE`; `renderCalRunConfirm()`, `renderCalRunning()`, `renderCalComplete()`

#### v1.8.0 — KS0108 GLCD Display
- **Display driver**: Replaced SSD1306 OLED (HW SPI) with original T962A KS0108 128x64 GLCD (8-bit parallel 6800 mode)
- **Pin mapping**: Complete reassignment to T962A original pinout — 8 data lines (GPIO 25/26/27/32/33/19/21/5), RS (GPIO 14), E (GPIO 2), CS1 (GPIO 15), CS2 (GPIO 13)
- ZC → GPIO 4, SSR → GPIO 16, cooling SSR → GPIO 17, system fan → GPIO 0, I2C SDA/SCL → GPIO 22/23
- Button pins: F1–F4 → GPIO 36–39, S → GPIO 34, E-STOP → GPIO 35, buzzer → GPIO 18
- `DisplayRenderer`: `U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI` → `U8G2_KS0108_128X64_1` with 14-pin parallel constructor
- Removed `setContrast()` — KS0108 has no contrast register
- `TemperatureReader`: `ADS1015_GAIN` → `GAIN_ONE` (Adafruit enum portability)

#### v1.9.0 — Line Frequency Auto-Calibration
- **Frequency measurement**: At boot, averages 40 ZC ISR edge timestamps via `esp_timer_get_time()` to determine AC mains frequency (45–66 Hz valid range; fallback to 60.0 Hz)
- **Dynamic PID timing**: `BRESENHAM_WINDOW_MS` and `PID_TIME_STEP_S` replaced with `getWindowMs()`/`getTimeStepS()` computed from measured half-cycle period
- **NVS persistence**: Measured frequency saved to NVS key `lineFreq` if >0.1 Hz difference from stored value
- **Splash screen**: Displays "Line: XX.X Hz" below firmware version
- `Config.h`: Removed `AC_FREQ_HZ`, `BRESENHAM_WINDOW_MS`, `PID_TIME_STEP_S`, `ZC_HALF_CYCLE_US_DEFAULT`; added `DEFAULT_FREQ_HZ`
- `BresenhamPID`: `calibrateLineFrequency()`, `CAL_CYCLES=40`, `setMeasuredFrequency()`/`getMeasuredFrequency()`

#### v2.0.0 — EMA Filtering, Feedforward Control, Plant Model Calibration
- **EMA temperature filter**: `TemperatureReader` applies α=0.15 EMA on each TC reading; removes ~85% of high-frequency LSB noise before it reaches PID derivative term; filter resets on each cycle start; fault detection still reads raw value
- **Target ramp rate**: `ProfileEngine::getTargetRampRate()` returns °C/s of the desired profile at any elapsed second; used as feedforward input
- **Feedforward control**: `computeFeedforward()` adds `dTargetDt / heatingRate[zone] * 256` to PID output; PID only corrects residual error; capped at 80% max output
- **Cooling feedforward**: `computeCoolingFeedforward()` activates proportional cooling only when target drops faster than natural cooling rate
- **Plant model as global property**: Heating rate, cooling rate, and deadtime stored globally in NVS (not per-recipe-per-zone); measured once by calibration test, used by feedforward on all subsequent runs
- **No continuous re-measurement**: `measureRampRate()` and `measureDeadtime()` called **only during calibration test**, not during normal reflow cycles; prevents estimator windup and false deadtime triggers
- **Calibration test measures plant**: Calibration routine now calls `measureRampRate()` per phase and `measureDeadtime()` on first heater step; saves global model via `saveGlobalPlantModel()` on completion
- **Cooldown time measurement**: Calibration run measures time from peak temp to 120°C (`CAL_COOLDOWN_TEMP`); saves to recipe as `cooldownTime` in NVS; used in `ProfileEngine::getTotalDuration()`
- **SharedData.h**: Added `cooldownTime` field to `ReflowRecipe` struct; `saveRecipeToNvs()`/`loadRecipeFromNvs()` persist `r{N}_cool` key
- `BresenhamPID`: `measureRampRate()`/`measureDeadtime()` moved to public; added `resetDeadtimeDetection()`; removed continuous measurement calls from `runPIDLoop`
- `main.cpp`: removed `savePlantModelForRecipeZone()`/`loadPlantModelForRecipeZone()`; added `saveGlobalPlantModel()`/`loadGlobalPlantModel()` with `phr0-4`, `pcr0-4`, `pdt` NVS keys
- `ProfileEngine.cpp`: `getTotalDuration()` uses `recipe.cooldownTime` instead of hardcoded 60s
- `DisplayRenderer`: `renderCalComplete()` shows "Cool:" (peak→120°C time) instead of "Hold:"
