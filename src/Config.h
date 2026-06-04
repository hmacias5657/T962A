#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define FIRMWARE_VERSION "2.1.0"

// T962A pin mappings (ESP32 WROOM)
#define PIN_ZC_INTERRUPT       GPIO_NUM_4
#define PIN_SSR_GATE           GPIO_NUM_16
#define PIN_COOLING_FAN_SSR    GPIO_NUM_17
#define PIN_BUZZER             GPIO_NUM_18
#define PIN_BTN_F1_UP          GPIO_NUM_36   // input-only pin
#define PIN_BTN_F2_DOWN        GPIO_NUM_37   // input-only pin
#define PIN_BTN_F3_LEFT        GPIO_NUM_38   // input-only pin
#define PIN_BTN_F4_RIGHT       GPIO_NUM_39   // input-only pin
#define PIN_BTN_S_SELECT       GPIO_NUM_34   // input-only pin
#define PIN_ESTOP              GPIO_NUM_35
#define PIN_SYS_FAN_PWM        GPIO_NUM_0    // 10k pull-up to 3.3V required
#define PIN_I2C_SDA            GPIO_NUM_22
#define PIN_I2C_SCL            GPIO_NUM_23

// KS0108 128x64 GLCD (8-bit parallel 6800 mode)
#define PIN_LCD_D0             GPIO_NUM_25
#define PIN_LCD_D1             GPIO_NUM_26
#define PIN_LCD_D2             GPIO_NUM_27
#define PIN_LCD_D3             GPIO_NUM_32
#define PIN_LCD_D4             GPIO_NUM_33
#define PIN_LCD_D5             GPIO_NUM_19
#define PIN_LCD_D6             GPIO_NUM_21
#define PIN_LCD_D7             GPIO_NUM_5
#define PIN_LCD_RS             GPIO_NUM_14   // Register Select (A0)
#define PIN_LCD_E              GPIO_NUM_2    // Enable strobe
#define PIN_LCD_CS1            GPIO_NUM_15   // Chip select – left half
#define PIN_LCD_CS2            GPIO_NUM_13   // Chip select – right half

// Backward-compatible aliases for old code
#define PIN_BTN_S              PIN_BTN_S_SELECT
#define PIN_BTN_ESTOP          PIN_ESTOP
#define PIN_DISPLAY_CS         PIN_LCD_CS1
#define PIN_DISPLAY_DC         PIN_LCD_RS
#define PIN_DISPLAY_RST        (-1)

// ADS1015
#define ADS1015_ADDR           0x48
#define ADS1015_FSR_VOLTS      4.096f
#define TC_AMP_SENSITIVITY     0.01f

// LEDC (system fan PWM) - Arduino API: channel 0-15, resolution 1-16 bits
#define SYS_FAN_PWM_CHAN       0
#define SYS_FAN_PWM_FREQ       25000
#define SYS_FAN_PWM_RES        8

// Bresenham PID
#define BRESENHAM_CYCLES       256
#define DEFAULT_FREQ_HZ        60.0f
#define PID_OUTPUT_MAX         BRESENHAM_CYCLES
#define PID_OUTPUT_MIN         0
#define PID_INTERVAL_MS_DEFAULT  2125
#define PID_ZONES              5
#define ZC_MEASURE_SAMPLES      20

// Safety limits
#define AMBIENT_TEMP            25.0f
#define OVERTEMP_C              280.0f
#define OVER_TEMP_LIMIT         OVERTEMP_C
#define SENSOR_FAULT_C          5.0f
#define SENSOR_FAULT_TEMP       SENSOR_FAULT_C
#define SPATIAL_DELTA_C         45.0f
#define MAX_SPATIAL_DELTA       SPATIAL_DELTA_C
#define MAX_BOARD_TEMP_C        85.0f

// Display
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          64
#define SCREEN_WIDTH            DISPLAY_WIDTH
#define SCREEN_HEIGHT           DISPLAY_HEIGHT
#define PLOT_X_START            20
#define PLOT_Y_BOTTOM           (DISPLAY_HEIGHT - 10)
#define PLOT_Y_MAX              280
#define PLOT_DURATION_SECS      480
#define PLOT_DURATION_S         PLOT_DURATION_SECS
#define TEMP_RANGE_MAX          PLOT_Y_MAX

// NVS
#define NVS_NAMESPACE          "reflow"
#define MAX_RECIPES            10
#define RECIPE_COUNT_MAX       MAX_RECIPES
#define RECIPE_COUNT_DEFAULT   3
#define MAX_RECIPE_NAME_LEN    24

// Button Debounce
#define DEBOUNCE_DELAY_MS      50

// Baking Profile Constants
#define BAKE_TEMP_MIN           80
#define BAKE_TEMP_MAX           150
#define BAKE_DURATION_DEFAULT   30
#define BAKE_DURATION_MIN       1
#define BAKE_DURATION_MAX_DEFAULT 300

// PID Zone Defaults
#define ZONE_KP_DEFAULT         2.0f
#define ZONE_KI_DEFAULT         0.05f
#define ZONE_KD_DEFAULT         1.2f
#define COOLING_ZONE_KP_DEFAULT 1.0f
#define COOLING_ZONE_KI_DEFAULT 0.01f
#define COOLING_ZONE_KD_DEFAULT 0.5f

// System Fan Constants
#define SYS_FAN_TEMP_MIN        40.0f
#define SYS_FAN_TEMP_MAX        80.0f

// Calibration Constants
#define CAL_OFFSET_MIN          -10.0f
#define CAL_OFFSET_MAX          10.0f
#define CAL_OFFSET_STEP         0.5f
#define CAL_OFFSET_STEP_INT     5
#define CAL_RUN_MARGIN          1.2f
#define CAL_PEAK_HOLD_S         10
#define CAL_COOLDOWN_TEMP       120
#define CAL_COOLDOWN_MIN        30

#endif
