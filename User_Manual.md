# User Manual: AI-Adaptive PID Reflow Oven Controller (T962A)

## 1. Overview

This controller automates the reflow soldering process in a T962A oven using an ESP32 with dual thermocouples, multi-zone adaptive PID control, a system cooling fan, a Bresenham-controlled oven cooling fan, and a 128x64 KS0108 GLCD display. It supports reflow profiles and baking/drying profiles with real-time temperature tracking, audible alerts, thermocouple calibration, and emergency stop. Heater and cooling PID coefficients are learned per stage and saved to NVS. The original T962A 5-key membrane (F1–F4, S) is reused with a dedicated hardware E-STOP added.

## 2. Getting Started

### 2.1 Power Up
1. Ensure mains power to the oven is connected through the zero-cross SSR.
2. Ensure the oven cooling fan SSR and system cooling fan are wired.
3. Plug in the 5V supply for the ESP32 controller.
4. The display shows a splash screen ("Reflow Oven / AI-PID v1.9.0 / Line: XX.X Hz") for 1.5 seconds, then enters the main menu. The line frequency is measured automatically at boot by averaging 40 zero-cross edge timestamps and is persisted in NVS if it differs from the previous measurement.

### 2.2 Main Menu
```
Reflow Oven Ctrl
────────────────
Recipe: 1
Hold [S] for start
[S] recipes
Ready
```

- **Recipe**: Indicates which stored profile is selected
- **[S] (short press)**: Enter the recipe selection menu to browse profiles
- **[S] (hold 1s)**: Begin the reflow cycle directly with the selected recipe

### 2.3 Selecting a Recipe / Mode

Press **S** (short press) to enter the recipe menu:

```
Select Recipe:
────────────────
>1:Sn63/Pb37  220C
 2:SAC305     245C
 3:Bi58/Sn42  170C
 Create New...
 Baking Profile
 Settings
 Calibration
```

- **F1 (UP) / F2 (DOWN)**: Navigate through recipes and menu options
- **F3 (LEFT) / F4 (RIGHT)**: Also navigate (same as UP/DOWN) in list views
- **S (short press)**: Launch the selected recipe immediately; enter profile creation if "Create New..." is selected; enter baking setup if "Baking Profile" is selected; enter settings if "Settings" is selected; enter calibration if "Calibration" is selected
- **E-STOP**: Return to main menu

### 2.4 Creating a New Reflow Profile

Scroll to **Create New...** and press **S** to enter the profile creation wizard:

```
Create Profile
Step 1/7
Preheat Temp: 100
min:60 max:200
```

- **F1 (UP) / F4 (RIGHT)**: Increase the current parameter value by ±5 (wraps around at min/max)
- **F2 (DOWN) / F3 (LEFT)**: Decrease the current parameter value by ±5 (wraps around at min/max)
- **S (short press)**: Confirm the value and advance to the next parameter
- **E-STOP**: Cancel profile creation and return to recipe select

The 7 parameters are edited in sequence:
1. Preheat Temperature (60-200°C)
2. Soak Temperature (80-220°C)
3. Peak Temperature (140-280°C)
4. Preheat Ramp Time (30-300 seconds)
5. Soak Time (30-300 seconds)
6. Reflow Time (20-180 seconds)
7. Peak Hold Time (5-120 seconds)

After the last parameter, the profile is saved to NVS with an auto-generated name and becomes immediately selectable for a new reflow session. Each new recipe initialises 5 zones (Preheat, Soak, Reflow Ramp, Reflow Peak, Cooldown) with default heater and cooling PID gains.

### 2.5 Pre-Programmed Recipes (defaults)

| # | Name | Preheat | Soak | Peak Temp | Ramp | Soak | Reflow | Hold | Use Case |
|---|------|---------|------|-----------|------|------|--------|------|----------|
| 1 | Sn63/Pb37 | 100°C | 150°C | 220°C | 90s | 90s | 30s | 30s | Leaded solder (standard) |
| 2 | SAC305 | 150°C | 200°C | 245°C | 90s | 90s | 30s | 30s | Lead-free RoHS solder |
| 3 | Bi58/Sn42 | 100°C | 140°C | 170°C | 90s | 90s | 30s | 30s | Low-temperature solder |

## 3. Running a Reflow Cycle

### 3.1 Start
1. Select the appropriate recipe for your solder paste.
2. Press **S** (short press) on the highlighted recipe in the recipe selection screen, or hold **S** for 1 second from the main menu.
3. A short beep confirms the cycle has begun.
4. The display switches to the live plot screen.

### 3.2 Live Plot Screen
```
1:211C 2:214C T:220C 142s
280┤
   │                  ,.--"/
   │             _.---'    `.
217─┼──────────-'            `.
200─┼──────_.-'               `.
   │      /                    .
150─┼_.-'
   │/
 25─┤
   └───┬──────┬──────┬──────┬──────┬──────┬──────┬──────┲━
     0   1      2      3      4      5      6      7   8(min)
   Preheat│  Soak   │  Reflow │  Cooling (coast to 0)
```

- **Top bar**: TC1 reading, TC2 reading, target temperature (Tg), elapsed time
- **Solid trace**: Dashed line = target profile; disc = current actual temperature
- **X-axis**: Time in minutes (0-8 min window, ticks at 0/1/2/3/4/5/6/7/8)
- **Y-axis**: Temperature from 0°C to 280°C with labels at 25/150/200/217/280
- **Horizontal lines**: Soak zone (150-200°C band), liquidus line (217°C)

- **Top bar**: Actual average temperature (T), target temperature (Tg), elapsed time
- **Solid trace**: Dashed line = target profile; disc = current actual temperature
- **X-axis**: Time in minutes (0-8 min window, ticks at 0/2/4/6/8 min)
- **Y-axis**: Temperature from 0°C to 280°C with labels at 0/140/280

The cycle proceeds through these stages automatically:

| Stage | Description |
|-------|-------------|
| Preheat | Ramp from ambient to 100-150°C at controlled rate |
| Soak | Flat plateau to activate flux and equalize board temperature |
| Reflow Ramp | Rapid ramp up to liquidus peak temperature |
| Reflow Peak | Hold at peak temperature for solder reflow |
| Cooldown | Controlled cooling via oven cooling fan to safe handling temperature |

A short beep sounds at each stage transition. On each transition, the current PID coefficients (Kp, Ki, Kd) for both heater and cooling fan are saved to the exiting zone in NVS, and the stored gains for the entering zone are loaded — the AI tuner then adapts from those starting values.

### 3.3 Cycle Complete
When the cooldown phase finishes (temperature below 50°C):
- Display shows "Cycle Complete"
- 3 long beeps sound (500ms each)
- The board is safe to remove once the temperature drops below 40°C
- The PID and cooling coefficients for the final zone are saved to NVS. Over successive runs, each zone converges to its optimal gain values.

## 4. Baking / Drying Profile

The controller also supports constant-temperature baking for drying PCBs, curing conformal coating, preheating, or solder paste drying.

### 4.1 Entering Baking Setup
1. Press **S** (short press) from the main menu to open recipe select.
2. Scroll to **Baking Profile** and press **S**.
3. The baking setup screen appears:

```
Baking Profile
────────────────
Set parameters:
>Temp: 100 C
 Time: 30 min
[S]=field [Hold]=start
```

### 4.2 Adjusting Baking Parameters
- The `>` marker indicates which field is active (Temp or Time)
- **S (short press)**: Toggle between temperature and duration fields
- **F1 (UP) / F4 (RIGHT)**: Increase the active field value
- **F2 (DOWN) / F3 (LEFT)**: Decrease the active field value
  - Temperature: adjusts by ±5°C (range 80–150°C)
  - Duration: adjusts by ±1 minute (range 1–300 minutes, upper bound configurable in Settings)
- **S (hold 1s)**: Begin the baking cycle
- **E-STOP**: Return to recipe select

### 4.3 Baking Running Screen
```
BAKING 105C / 100C
Time: 5m30s / 30m
[████████░░░░░░░░░░]
[E-STOP] to abort
```

- **Top line**: Current average temperature / target temperature
- **Time**: Elapsed time / total bake duration
- **Progress bar**: Visual indication of remaining time
- **E-STOP**: Aborts the baking cycle immediately

### 4.4 Bake Complete
When the timer expires:
- Display shows the bake running screen with elapsed time at maximum
- 3 long beeps sound
- Press **S** or **E-STOP** to return to the main menu

## 5. Settings Menu

The settings menu allows you to configure the maximum bake duration and choose temperature units (°C or °F).

### 5.1 Entering Settings
1. Press **S** (short press) from the main menu to open recipe select.
2. Scroll to **Settings** and press **S**.
3. The settings screen appears:

```
Settings
────────────────
>Max Bake: 300 min
 Units: C
[S]=row [Hold]=save
```

### 5.2 Adjusting Settings
- **S (short press)**: Toggle the cursor (`>`) between the two rows
- **F1 (UP) / F4 (RIGHT)**:
  - **Max Bake** row: increase by ±5 minutes (wraps at 1 and 300)
  - **Units** row: toggle between `C` (Celsius) and `F` (Fahrenheit)
- **F2 (DOWN) / F3 (LEFT)**:
  - **Max Bake** row: decrease by ±5 minutes (wraps at 1 and 300)
  - **Units** row: toggle between `C` and `F`
- **S (hold 1s)**: Save both settings to NVS and return to recipe select

### 5.3 Temperature Unit Display
When °F is selected, all displayed temperatures are converted:
- Live plot: `1:212 2:217 T:302F 45s`
- Bake running: `1:212 2:217 /302F`
- Bake setup: `Temp: 212 F`
- Recipe select: peak temps shown with `F` suffix
- Profile creation: temperature parameters (steps 1–3) show °F values with °F suffix; time parameters (steps 4–7) remain unchanged

Internal control, PID calculation, safety limits, and profile engine always operate in °C — only the display layer converts.

## 6. Calibration Menu

The calibration menu allows you to adjust per-sensor thermocouple offsets and view the stored per-zone PID gains for the selected recipe.

### 6.1 Entering Calibration
1. Press **S** (short press) from the main menu to open recipe select.
2. Scroll to **Calibration** and press **S**.
3. The calibration screen appears:

```
Calibration
────────────────
>TC1 Off:+0.0C
 TC2 Off:+0.0C
 View Gains
 Cal Run
[S]=edit [Hold]=save
```

### 6.2 Adjusting Thermocouple Offsets
- **F1 (UP) / F2 (DOWN)**: Move the `>` cursor between rows (4 items: TC1 Offset, TC2 Offset, View Gains, Cal Run)
- **F3 (LEFT) / F4 (RIGHT)**: Also move cursor (same as UP/DOWN)
- **S (short press)**: Toggle edit mode on the selected offset row — "EDIT" appears next to the value when active
- **F1 (UP) / F4 (RIGHT)** (edit mode): Increase the offset by ±0.5°C steps (range -10.0°C to +10.0°C)
- **F2 (DOWN) / F3 (LEFT)** (edit mode): Decrease the offset by ±0.5°C steps (range -10.0°C to +10.0°C)
- **S (short press)** (edit mode): Exit edit mode
- **S (hold 1s)**: Save both offsets to NVS and return to recipe select
- **E-STOP**: Discard changes and return to recipe select

Offsets are applied immediately to temperature readings and persist across reboots.

### 6.3 Viewing Zone PID Gains
With the cursor on "View Gains", press **S** (short press) to enter the zone gain viewer:

```
Zone:0 PRE  1/5
Heater
Kp:2.40 Ki:0.025
Kd:0.96
Cooling
Kp:0.50 Ki:0.005
Kd:0.20
```

- **F1 (UP) / F4 (RIGHT)**: Cycle forward through zones 0–4 (PRE, SOAK, RAMP, PEAK, COOL)
- **F2 (DOWN) / F3 (LEFT)**: Cycle backward through zones
- **S** or **E-STOP**: Return to calibration main screen

Both heater and cooling PID gains for each zone are displayed and are recipe-specific.

### 6.4 Calibration Run (Auto-Tune Recipe Times)

With the cursor on "Cal Run", press **S** (short press) to enter the calibration run confirmation screen:

```
Calibration Run
────────────────
Recipe: Sn63/Pb37
Measures ramp times
then updates profile.
[S]=start [E-STOP]=back
```

Press **S** (short press) to begin the calibration run — a dry cycle that measures the oven's actual heating performance:

```
Cal Run
────────────────
Phase: Preheat
Target: 100C
Actual: 85C
Time: 45s
```

The run proceeds through 5 phases automatically:

| Phase | Description |
|-------|-------------|
| Preheat | Heats aggressively to the recipe's preheat temp; records time |
| Soak | Heats from preheat temp to soak temp; records time |
| Reflow | Heats from soak temp to peak temp; records time |
| Hold | Brief 10-second hold at peak temperature |
| Cooling | Fan cooling until safe temperature |

After cooldown, updated recipe times are displayed:

```
Cal Complete
────────────────
Sn63/Pb37 updated
Ramp:144s Soak:144s
Reflow:72s Hold:30s
[S]=back to menu
```

Each measured time is multiplied by a 1.2x safety margin (minimums: Ramp 30s, Soak 30s, Reflow 20s, Hold 30s). The updated recipe is saved to NVS and becomes available for immediate use.

The oven must be empty (no PCB) during a calibration run. E-STOP is available at any time.

## 7. Multi-Zone PID Learning

Each recipe has 5 independent PID gain sets for both heater and cooling, one per reflow stage:

### Heater Gains

| Zone | Default Kp | Default Ki | Default Kd | Characteristic |
|------|-----------|-----------|-----------|----------------|
| Preheat | 2.40 | 0.025 | 0.96 | Moderate P, low I, moderate D — avoids overshoot on initial ramp |
| Soak | 1.60 | 0.075 | 0.60 | Lower P, higher I — steady-state regulation on plateau |
| Reflow Ramp | 2.00 | 0.050 | 1.56 | Neutral P, high D — fast ramp-up with damping |
| Reflow Peak | 1.20 | 0.015 | 2.16 | Low P, very low I, aggressive D — tight peak hold |
| Cooldown | 0.20 | 0.000 | 0.12 | Minimal control — natural cooling, just avoids thermal shock |

### Cooling Fan Gains

| Zone | Default Kp | Default Ki | Default Kd | Characteristic |
|------|-----------|-----------|-----------|----------------|
| Preheat | 0.50 | 0.005 | 0.20 | Low — minimal cooling needed during initial ramp |
| Soak | 0.50 | 0.005 | 0.20 | Low — minimal cooling needed during plateau |
| Reflow Ramp | 1.00 | 0.010 | 0.50 | Moderate — handles overshoot suppression during ramp |
| Reflow Peak | 1.50 | 0.020 | 0.80 | Moderate-high — tight overshoot protection at peak |
| Cooldown | 2.00 | 0.030 | 1.00 | Primary cooling control during cooldown phase |

When the control loop detects a stage transition:
1. Current BresenhamPID heater and cooling gains are saved to the exiting zone's NVS slot.
2. Stored heater and cooling gains for the new zone are loaded into the PID controller.
3. The AI tuner applies spatial damping and a learning heuristic for both heater and cooling during the run.
4. At cycle end, the current gains are saved to the final zone.

Over multiple cycles, each zone accumulates its own optimal gain values independent of the other zones.

## 8. Cooling Fans

### 8.1 System Cooling Fan (GPIO 0)
The system cooling fan keeps the ESP32 controller electronics cool:
- Fan speed is proportional to the oven's average temperature
- At 40°C: fan off (0% duty)
- At 80°C: fan at full speed (100% duty)
- Linear interpolation between 40°C and 80°C
- Controlled via PWM through an N-channel logic-level MOSFET
- No user intervention required — operates automatically

### 8.2 Oven Cooling Fan (GPIO 17)
The oven cooling fan assists the PID algorithm in controlling temperature:
- Controlled via a dedicated zero-cross SSR using the Bresenham power distribution algorithm
- During the **Cooldown** stage: the fan modulates proportionally to track the decreasing target temperature curve
- During **overshoot** events (temperature exceeds target by >10°C in any stage): the fan activates to suppress the overshoot while the heater turns off
- Cooling power is distributed over 256 half-cycles, same as the heater, for smooth linear control
- The AITuner adapts cooling gains spatially — more cooling power when the gradient between sensors is high

## 9. Emergency Stop

Press the dedicated hardware **E-STOP** button at any time during a reflow or baking cycle to immediately:

1. Set heater SSR output to 0% (heaters off)
2. Set cooling fan SSR output to 0% (fan off)
3. Display the EMERGENCY STOP screen
4. Sound a continuous alarm tone

```
EMERGENCY STOP
Reset to continue
```

**To reset**: Press **S** (short press) to acknowledge the emergency stop and return to the main menu.

## 10. Error Conditions

### 10.1 Error Screen
```
ERROR
Code: 1
TC Open Circuit
```

Press **S** (short press) to acknowledge and return to the main menu.

### 10.2 Error Codes

| Code | Condition | Buzzer | Action Required |
|------|-----------|--------|-----------------|
| 1 | Thermocouple open-circuit or disconnected (< 5°C reading) | Rapid intermittent beeps | Check thermocouple wiring and connections |
| 2 | Thermal runaway (> 280°C detected on either sensor) | Continuous tone | Power cycle the system; check SSR and heating elements |
| 3 | Excessive thermal gradient (> 45°C difference between sensors) | Rapid intermittent beeps | Ensure both thermocouples are properly placed in the oven |

If a sensor consistently reads slightly off, use the **Calibration** menu to apply a per-sensor offset (±10°C) instead of replacing hardware.

## 11. Buzzer Reference

| Sound | Meaning |
|-------|---------|
| 1 short beep (150ms) | Phase transition (e.g., Soak -> Reflow) |
| 3 long beeps (500ms) | Cycle / bake complete, safe to open drawer |
| Rapid intermittent (100ms on/off) | Error: sensor fault (code 1 or 3) |
| Continuous tone (250ms on/off) | Error: thermal runaway (code 2) or E-Stop active |

## 12. Safety Precautions

- **Never leave the oven unattended** during a reflow or baking cycle.
- Ensure the dedicated hardware E-STOP button is easily accessible before starting any cycle.
- The oven reaches temperatures above 250°C. Use appropriate heat protection.
- Verify thermocouple placement before each cycle — poor contact causes inaccurate readings.
- If the system enters an error state, investigate the cause before restarting.
- Always allow the oven to cool below 40°C before opening the drawer.
- Ensure the oven cooling fan is unobstructed — it is essential for controlled cooldown.
- Verify both SSRs (heater and cooling fan) have adequate heatsinking.

## 13. Maintenance

- Periodically check thermocouple connections for oxidation or loose contacts.
- Keep the display and button panel free of flux residue.
- Verify the SSRs are properly heatsinked — a failed SSR (stuck closed) can cause thermal runaway.
- Clean the system cooling fan periodically to prevent dust buildup.
- After firmware updates, verify calibration offsets in the Calibration menu and test with a dry run (no PCB) before production use.

## 14. Troubleshooting

| Problem | Possible Cause | Solution |
|---------|---------------|----------|
| Display shows "TC Open Circuit" | Thermocouple disconnected or faulty | Check wiring; verify amplifier IC power |
| Temperature reading stuck at ~25°C | ADC scaling incorrect or sensor fault | Verify amplifier gain; check ADC pin voltage |
| Oven not heating | Heater SSR not firing, zero-cross signal missing | Check SSR gate wiring; verify ZC interrupt pin (GPIO 4) |
| Oven cooling fan not running during cooldown | Cooling SSR wiring or pin (GPIO 17) | Check SSR gate and power to cooling fan |
| Large overshoot at peak | PID gains too aggressive | The AI tuner adapts over successive cycles per zone |
| One sensor reads much hotter | Uneven heating or sensor placement | Reposition thermocouples symmetrically; use Calibration offsets |
| Sensor reads consistently off | Thermocouple or amplifier tolerance | Use Calibration menu to apply offset (±0.5°C steps) |
| Continuous error on startup | Hardware fault | Check all connections; reflash firmware |
| Baking profile not reaching set temp | Duration too short for mass | Increase bake duration in setup |
