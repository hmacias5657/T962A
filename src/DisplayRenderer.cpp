#include "DisplayRenderer.h"
#include <U8g2lib.h>

DisplayRenderer::DisplayRenderer()
    : _display(U8G2_R0,
               PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
               PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7,
               PIN_LCD_E, PIN_LCD_RS, PIN_LCD_CS1, PIN_LCD_CS2,
               U8X8_PIN_NONE, U8X8_PIN_NONE),
      _menuState(MENU_MAIN), _useFahrenheit(false), _measuredFreq(0.0f) {}

void DisplayRenderer::begin() {
    _display.begin();
    _display.setFont(u8g2_font_5x7_tf);
}

int DisplayRenderer::toDisplayTemp(float celsius) const {
    if (_useFahrenheit) {
        return (int)(celsius * 9.0f / 5.0f + 32.0f);
    }
    return (int)celsius;
}

int DisplayRenderer::mapTimeToX(int seconds) const {
    return PLOT_X_START + ((seconds * (SCREEN_WIDTH - PLOT_X_START)) / PLOT_DURATION_S);
}

int DisplayRenderer::mapTempToY(float temp) const {
    int y = PLOT_Y_BOTTOM - ((int)(temp * PLOT_Y_BOTTOM) / (int)TEMP_RANGE_MAX);
    return constrain(y, 0, PLOT_Y_BOTTOM);
}

void DisplayRenderer::drawAxes() {
    _display.drawHLine(PLOT_X_START, PLOT_Y_BOTTOM, SCREEN_WIDTH - PLOT_X_START);
    _display.drawVLine(PLOT_X_START, 9, PLOT_Y_BOTTOM - 9);

    for (int t = 0; t <= PLOT_DURATION_S; t += 120) {
        int x = mapTimeToX(t);
        _display.drawPixel(x, PLOT_Y_BOTTOM + 1);
        _display.setCursor(x - 3, SCREEN_HEIGHT - 1);
        _display.print(t / 60);
    }

    _display.setCursor(0, 15);
    _display.print("280");
    _display.setCursor(0, 35);
    _display.print("140");
    _display.setCursor(5, 57);
    _display.print("0");
}

void DisplayRenderer::drawTargetProfile(const ReflowRecipe &recipe) {
    if (recipe.type == RECIPE_BAKE) return;
    for (int t = 0; t < PLOT_DURATION_S; t += 5) {
        int targetTemp = _profile.calculateTargetTemp(t, recipe);
        int x = mapTimeToX(t);
        int y = mapTempToY(targetTemp);

        if (t % 10 == 0) {
            _display.drawPixel(x, y);
        }
    }
}

void DisplayRenderer::renderMainMenu(int selectedRecipe) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(10, 12, "Reflow Oven Ctrl");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(5, 30, "Recipe:");
        _display.setCursor(50, 30);
        _display.print(selectedRecipe + 1);

        _display.drawStr(5, 42, "Hold [S] for start");
        _display.drawStr(5, 54, "[S] recipes");

        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(5, SCREEN_HEIGHT - 2, "Ready");
    } while (_display.nextPage());
}

void DisplayRenderer::renderRecipeSelect(int selectedIndex, const ReflowRecipe *recipes, int count, bool showCreate, bool showBake, bool showSettings, bool showCalibration) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 12, "Select Recipe:");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        int lineIdx = 0;

        for (int i = 0; i < count; i++) {
            int y = 30 + (lineIdx * 10);
            if (i == selectedIndex) _display.drawStr(3, y, ">");
            _display.setCursor(13, y);
            _display.print(i + 1);
            _display.print(":");
            _display.print(recipes[i].name);
            _display.print(" ");
            _display.print(toDisplayTemp(recipes[i].peakTemp));
            if (_useFahrenheit) _display.print("F"); else _display.print("C");
            lineIdx++;
        }

        if (showCreate) {
            int y = 30 + (lineIdx * 10);
            if (count == selectedIndex) _display.drawStr(3, y, ">");
            _display.setCursor(13, y);
            _display.print("Create New...");
            lineIdx++;
        }

        if (showBake) {
            int bakeIdx = count + (showCreate ? 1 : 0);
            int y = 30 + (lineIdx * 10);
            if (bakeIdx == selectedIndex) _display.drawStr(3, y, ">");
            _display.setCursor(13, y);
            _display.print("Baking Profile");
            lineIdx++;
        }

        if (showSettings) {
            int settingsIdx = count + (showCreate ? 1 : 0) + (showBake ? 1 : 0);
            int y = 30 + (lineIdx * 10);
            if (settingsIdx == selectedIndex) _display.drawStr(3, y, ">");
            _display.setCursor(13, y);
            _display.print("Settings");
            lineIdx++;
        }

        if (showCalibration) {
            int calIdx = count + (showCreate ? 1 : 0) + (showBake ? 1 : 0) + (showSettings ? 1 : 0);
            int y = 30 + (lineIdx * 10);
            if (calIdx == selectedIndex) _display.drawStr(3, y, ">");
            _display.setCursor(13, y);
            _display.print("Calibration");
            lineIdx++;
        }

        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(5, SCREEN_HEIGHT - 2, "[S] start [E-STOP] back");
    } while (_display.nextPage());
}

void DisplayRenderer::renderLivePlot(const ThermalTelemetry &data, const ReflowRecipe &recipe) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(0, 8);
        _display.print("1:");
        _display.print(toDisplayTemp(data.tempTC1));
        _display.print(" 2:");
        _display.print(toDisplayTemp(data.tempTC2));
        _display.print(" T:");
        _display.print(toDisplayTemp(data.targetTemp));
        if (_useFahrenheit) _display.print("F"); else _display.print("C");
        _display.print(" ");
        _display.print(data.currentSeconds);
        _display.print("s");

        if (data.currentSeconds < PLOT_DURATION_S) {
            drawAxes();
            if (recipe.type == RECIPE_REFLOW) {
                drawTargetProfile(recipe);
            }

            int liveX = mapTimeToX(data.currentSeconds);
            int liveY = mapTempToY(data.avgTemp);
            _display.drawDisc(liveX, liveY, 2);

            _display.setCursor(SCREEN_WIDTH - 15, PLOT_Y_BOTTOM + 9);
            _display.print(data.currentSeconds);
        } else {
            _display.setCursor(20, 35);
            if (recipe.type == RECIPE_BAKE) {
                _display.print("Bake Complete");
            } else {
                _display.print("Cycle Complete");
            }
        }
    } while (_display.nextPage());
}

void DisplayRenderer::renderErrorScreen(uint8_t errorCode) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(15, 20, "ERROR");

        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(5, 35);
        _display.print("Code: ");
        _display.print(errorCode);

        const char *msg = "";
        switch (errorCode) {
            case 1: msg = "TC Open Circuit"; break;
            case 2: msg = "Thermal Runaway"; break;
            case 3: msg = "TC Fault"; break;
            default: msg = "Unknown Error"; break;
        }
        _display.drawStr(5, 50, msg);
    } while (_display.nextPage());
}

void DisplayRenderer::renderProfileCreate(int editStep, int totalSteps, const char *label, int value, int minVal, int maxVal, bool isTemperature) {
    int displayVal = value;
    int displayMin = minVal;
    int displayMax = maxVal;

    if (isTemperature && _useFahrenheit) {
        displayVal = toDisplayTemp(value);
        displayMin = toDisplayTemp(minVal);
        displayMax = toDisplayTemp(maxVal);
    }

    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 12, "Create Profile");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(5, 30);
        _display.print("Step ");
        _display.print(editStep + 1);
        _display.print("/");
        _display.print(totalSteps);

        _display.setCursor(5, 45);
        _display.print(label);
        _display.print(":");

        char valStr[8];
        itoa(displayVal, valStr, 10);
        _display.setCursor(60, 45);
        _display.print(valStr);
        if (isTemperature) {
            if (_useFahrenheit) _display.print("F"); else _display.print("C");
        }

        _display.setCursor(5, 58);
        _display.print("min:");
        _display.print(displayMin);
        if (isTemperature) {
            if (_useFahrenheit) _display.print("F"); else _display.print("C");
        }
        _display.print(" max:");
        _display.print(displayMax);
        if (isTemperature) {
            if (_useFahrenheit) _display.print("F"); else _display.print("C");
        }
    } while (_display.nextPage());
}

void DisplayRenderer::renderBakeSetup(int bakeTemp, int bakeDuration, int maxDuration, bool editingTemp) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 12, "Baking Profile");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(5, 22, "Set parameters:");

        if (editingTemp) {
            _display.drawStr(3, 35, ">");
        } else {
            _display.drawStr(3, 48, ">");
        }

        _display.setCursor(13, 35);
        _display.print("Temp: ");
        _display.print(toDisplayTemp(bakeTemp));
        if (_useFahrenheit) _display.print(" F"); else _display.print(" C");

        _display.setCursor(13, 48);
        _display.print("Time: ");
        _display.print(bakeDuration);
        _display.print(" min");

        _display.setCursor(5, 62);
        _display.print("[S]=field [Hold]=start");
    } while (_display.nextPage());
}

void DisplayRenderer::renderBakeRunning(float tempTC1, float tempTC2, int targetTemp, int elapsedSec, int totalSec) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(0, 8);
        _display.print("1:");
        _display.print(toDisplayTemp(tempTC1));
        _display.print(" 2:");
        _display.print(toDisplayTemp(tempTC2));
        _display.print(" /");
        _display.print(toDisplayTemp(targetTemp));
        if (_useFahrenheit) _display.print("F"); else _display.print("C");

        int elapsedMin = elapsedSec / 60;
        int elapsedSecRem = elapsedSec % 60;
        int totalMin = totalSec / 60;

        _display.setCursor(0, 20);
        _display.print("Time: ");
        _display.print(elapsedMin);
        _display.print("m");
        _display.print(elapsedSecRem);
        _display.print("s / ");
        _display.print(totalMin);
        _display.print("m");

        int progressX = (elapsedSec * (SCREEN_WIDTH - 10)) / totalSec;
        progressX = constrain(progressX, 0, SCREEN_WIDTH - 10);
        _display.drawFrame(5, 30, SCREEN_WIDTH - 10, 10);
        _display.drawBox(6, 31, progressX, 8);

        _display.setCursor(5, 55);
        _display.print("[E-STOP] to abort");
    } while (_display.nextPage());
}

void DisplayRenderer::renderSettings(int maxBakeMinutes, int selectedRow, bool useFahrenheit) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 12, "Settings");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        if (selectedRow == 0) _display.drawStr(3, 30, ">");
        _display.setCursor(13, 30);
        _display.print("Max Bake: ");
        _display.print(maxBakeMinutes);
        _display.print(" min");

        if (selectedRow == 1) _display.drawStr(3, 44, ">");
        _display.setCursor(13, 44);
        _display.print("Units: ");
        if (useFahrenheit) _display.print("F"); else _display.print("C");

        _display.setCursor(5, 60);
        _display.print("[S]=row [Hold]=save");
    } while (_display.nextPage());
}

void DisplayRenderer::renderEStopScreen() {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(10, 25, "EMERGENCY STOP");
        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(10, 45, "Reset to continue");
    } while (_display.nextPage());
}

void DisplayRenderer::renderSplash() {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(10, 20, "Reflow Oven");
        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(15, 34, "AI-PID v");
        _display.print(FIRMWARE_VERSION);

        _display.setCursor(15, 48);
        if (_measuredFreq > 0.0f) {
            _display.print("Line: ");
            _display.print(_measuredFreq, 1);
            _display.print(" Hz");
        } else {
            _display.print("Calibrating...");
        }
    } while (_display.nextPage());
}

void DisplayRenderer::renderCalibration(int selectedItem, bool editMode, float tc1Offset, float tc2Offset) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 10, "Calibration");
        _display.drawHLine(0, 13, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        char buf[20];

        if (selectedItem == 0) _display.drawStr(3, 22, ">");
        _display.setCursor(13, 22);
        _display.print("TC1 Off:");
        snprintf(buf, sizeof(buf), "%+.1f", (double)tc1Offset);
        _display.print(buf);
        _display.print("C");
        if (selectedItem == 0 && editMode) {
            _display.drawStr(95, 22, "EDIT");
        }

        if (selectedItem == 1) _display.drawStr(3, 31, ">");
        _display.setCursor(13, 31);
        _display.print("TC2 Off:");
        snprintf(buf, sizeof(buf), "%+.1f", (double)tc2Offset);
        _display.print(buf);
        _display.print("C");
        if (selectedItem == 1 && editMode) {
            _display.drawStr(95, 31, "EDIT");
        }

        if (selectedItem == 2) _display.drawStr(3, 40, ">");
        _display.setCursor(13, 40);
        _display.print("View Gains");

        if (selectedItem == 3) _display.drawStr(3, 49, ">");
        _display.setCursor(13, 49);
        _display.print("Cal Run");

        _display.setCursor(5, 61);
        _display.print("[S]=edit [Hold]=save");
    } while (_display.nextPage());
}

void DisplayRenderer::renderZoneGains(int zone, const PidGains &heaterGains, const PidGains &coolingGains) {
    static const char* zoneLabels[] = {"PRE", "SOAK", "RAMP", "PEAK", "COOL"};
    const char* zl = (zone >= 0 && zone < NUM_ZONES) ? zoneLabels[zone] : "?";

    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.setCursor(5, 12);
        _display.print("Zone:");
        _display.print(zone);
        _display.print(" ");
        _display.print(zl);

        _display.setFont(u8g2_font_5x7_tf);
        _display.drawStr(5, 24, "Heater");
        _display.setCursor(5, 34);
        _display.print("Kp:");
        _display.print(heaterGains.Kp, 2);
        _display.setCursor(60, 34);
        _display.print("Ki:");
        _display.print(heaterGains.Ki, 3);
        _display.setCursor(5, 44);
        _display.print("Kd:");
        _display.print(heaterGains.Kd, 2);

        _display.drawStr(5, 54, "Cooling");
        _display.setCursor(5, 63);
        _display.print("Kp:");
        _display.print(coolingGains.Kp, 2);
        _display.setCursor(60, 63);
        _display.print("Ki:");
        _display.print(coolingGains.Ki, 3);
        _display.setCursor(100, 63);
        _display.print("Kd:");
        _display.print(coolingGains.Kd, 2);

        _display.setCursor(110, 12);
        _display.print("/");
        _display.print(NUM_ZONES);
    } while (_display.nextPage());
}

void DisplayRenderer::renderCalRunConfirm(const char *recipeName) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 12, "Calibration Run");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(5, 30);
        _display.print("Recipe: ");
        _display.print(recipeName);

        _display.drawStr(5, 44, "Measures ramp times");
        _display.drawStr(5, 54, "then updates profile.");
        _display.setCursor(5, 63);
        _display.print("[S]=start [E-STOP]=back");
    } while (_display.nextPage());
}

void DisplayRenderer::renderCalRunning(int targetTemp, float avgTemp, int elapsedSec, int phase) {
    static const char *phaseLabels[] = {"Preheat", "Soak", "Reflow", "Hold", "Cooling"};
    const char *pl = (phase >= 0 && phase < 5) ? phaseLabels[phase] : "?";

    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 12, "Cal Run");
        _display.drawHLine(0, 16, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(5, 28);
        _display.print("Phase: ");
        _display.print(pl);

        _display.setCursor(5, 38);
        _display.print("Target: ");
        _display.print(targetTemp);
        if (_useFahrenheit) _display.print("F"); else _display.print("C");

        _display.setCursor(5, 48);
        _display.print("Actual: ");
        _display.print((int)avgTemp);
        if (_useFahrenheit) _display.print("F"); else _display.print("C");

        _display.setCursor(5, 58);
        _display.print("Time: ");
        _display.print(elapsedSec);
        _display.print("s");
    } while (_display.nextPage());
}

void DisplayRenderer::renderCalComplete(const char *recipeName, int preheatTime, int soakTime, int reflowTime, int holdTime) {
    _display.firstPage();
    do {
        _display.setFont(u8g2_font_6x10_tf);
        _display.drawStr(5, 10, "Cal Complete");
        _display.drawHLine(0, 13, SCREEN_WIDTH);

        _display.setFont(u8g2_font_5x7_tf);
        _display.setCursor(5, 22);
        _display.print(recipeName);
        _display.print(" updated");

        _display.setCursor(5, 32);
        _display.print("Ramp:");
        _display.print(preheatTime);
        _display.print("s");

        _display.setCursor(60, 32);
        _display.print("Soak:");
        _display.print(soakTime);
        _display.print("s");

        _display.setCursor(5, 42);
        _display.print("Reflow:");
        _display.print(reflowTime);
        _display.print("s");

        _display.setCursor(60, 42);
        _display.print("Hold:");
        _display.print(holdTime);
        _display.print("s");

        _display.setCursor(5, 60);
        _display.print("[S]=back to menu");
    } while (_display.nextPage());
}
