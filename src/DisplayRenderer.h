#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include <U8g2lib.h>
#include "Config.h"
#include "SharedData.h"
#include "ProfileEngine.h"

enum MenuState : uint8_t {
    MENU_MAIN = 0,
    MENU_RECIPE_SELECT,
    MENU_RUNNING,
    MENU_ERROR,
    MENU_ESTOP,
    MENU_PROFILE_CREATE,
    MENU_BAKE_SETUP,
    MENU_BAKE_RUNNING,
    MENU_SETTINGS,
    MENU_CALIBRATION,
    MENU_CALIBRATION_GAINS,
    MENU_CAL_RUN_CONFIRM,
    MENU_CAL_RUNNING,
    MENU_CAL_COMPLETE,
    MENU_CALIBRATION_PLANT
};

class DisplayRenderer {
public:
    DisplayRenderer();

    void begin();
    void setUseFahrenheit(bool enabled) { _useFahrenheit = enabled; }
    void setMeasuredFrequency(float freq) { _measuredFreq = freq; }
    int toDisplayTemp(float celsius) const;

    void renderMainMenu(int selectedRecipe);
    void renderRecipeSelect(int selectedIndex, const ReflowRecipe *recipes, int count, bool showCreate, bool showBake, bool showSettings, bool showCalibration);
    void renderLivePlot(const ThermalTelemetry &data, const ReflowRecipe &recipe);
    void renderErrorScreen(uint8_t errorCode);
    void renderEStopScreen();
    void renderSplash();
    void renderProfileCreate(int editStep, int totalSteps, const char *label, int value, int minVal, int maxVal, bool isTemperature);
    void renderBakeSetup(int bakeTemp, int bakeDuration, int maxDuration, bool editingTemp);
    void renderBakeRunning(float tempTC1, float tempTC2, int targetTemp, int elapsedSec, int totalSec);
    void renderSettings(int maxBakeMinutes, int selectedRow, bool useFahrenheit, float lineFreq);
    void renderPlantModel(int zone, float heatRate, float coolRate, float deadtime);
    void renderCalibration(int selectedItem, bool editMode, float tc1Offset, float tc2Offset);
    void renderZoneGains(int zone, const PidGains &heaterGains, const PidGains &coolingGains);
    void renderCalRunConfirm(const char *recipeName);
    void renderCalRunning(int targetTemp, float avgTemp, int elapsedSec, int phase);
    void renderCalComplete(const char *recipeName, int preheatTime, int soakTime, int reflowTime, int holdTime, int cooldownTime);

private:
    U8G2_KS0108_128X64_1 _display;
    MenuState _menuState;
    ProfileEngine _profile;
    bool _useFahrenheit;
    float _measuredFreq;

    int mapTimeToX(int seconds) const;
    int mapTempToY(float temp) const;
    void drawAxes();
    void drawTargetProfile(const ReflowRecipe &recipe);
};

#endif
