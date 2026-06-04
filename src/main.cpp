#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "SharedData.h"
#include "BresenhamPID.h"
#include "TemperatureReader.h"
#include "ProfileEngine.h"
#include "AITuner.h"
#include "ButtonDebouncer.h"
#include "DisplayRenderer.h"
#include "Buzzer.h"

// ===================== Global Objects =====================
BresenhamPID bresenhamPID;
TemperatureReader tempReader;
ProfileEngine profileEngine;
AITuner aiTuner;
ButtonDebouncer buttonDebouncer;
DisplayRenderer display;
Buzzer buzzer;

// ===================== FreeRTOS Handles =====================
TaskHandle_t core0TaskHandle = NULL;
TaskHandle_t core1TaskHandle = NULL;
QueueHandle_t telemetryQueue = NULL;
SemaphoreHandle_t recipeMutex = NULL;

// ===================== NVS Storage =====================
Preferences prefs;
int storedRecipeCount = 0;

// ===================== Shared State =====================
volatile SystemState currentSystemState = STATE_IDLE;
volatile uint8_t currentErrorCode = 0;
ReflowRecipe activeRecipe;
ReflowRecipe recipes[RECIPE_COUNT_MAX];
PidGains recipePidGains[RECIPE_COUNT_MAX][NUM_ZONES];
PidGains recipeCoolingGains[RECIPE_COUNT_MAX][NUM_ZONES];
int selectedRecipeIndex = 0;
int profileTimerSeconds = 0;
int lastActiveRecipeIndex = 0;

// ===================== Bake State =====================
int bakeTargetTemp = 100;
int bakeTargetDurationMin = 30;
int bakeTotalSeconds = 0;
bool bakeEditingTemp = true;

// ===================== Settings State =====================
int settingsMaxBakeMinutes = BAKE_DURATION_MAX_DEFAULT;
int settingsSelectedRow = 0;
bool useFahrenheit = false;

// ===================== Calibration State =====================
float tc1Offset = 0.0f;
float tc2Offset = 0.0f;
int calibrationSelectedItem = 0;
bool calibrationEditMode = false;
int calibrationViewZone = 0;

// ===================== Calibration Run State =====================
int calPreheatTime = 0;
int calSoakTime = 0;
int calReflowTime = 0;
int calPeakReachedSec = 0;
int calCooldownTime = 0;
int calTargetRecipeIndex = 0;
int calPhase = 0;

// ===================== S Key Long-Press State =====================
static unsigned long sKeyPressStart = 0;
static bool sKeyIsDown = false;
static bool sKeyLongHandled = false;

static bool isSShortPress() {
    bool reading = digitalRead(PIN_BTN_S) == LOW;
    if (!reading && sKeyIsDown && !sKeyLongHandled) {
        sKeyIsDown = false;
        sKeyLongHandled = false;
        return true;
    }
    if (reading && !sKeyIsDown) {
        sKeyIsDown = true;
        sKeyPressStart = millis();
        sKeyLongHandled = false;
    }
    return false;
}

static bool isSLongPress() {
    if (sKeyIsDown && !sKeyLongHandled && (millis() - sKeyPressStart >= 1000)) {
        sKeyLongHandled = true;
        return true;
    }
    return false;
}

// ===================== Zone Transition Tracking =====================
ProfileStage previousControlStage = STAGE_COMPLETE;

// ===================== Profile Creation State =====================
bool profileCreateActive = false;
int profileCreateStep = 0;
const int PROFILE_CREATE_STEPS = 7;
int profileCreateValues[7];
const char* profileCreateLabels[7] = {
    "Preheat Temp", "Soak Temp", "Peak Temp",
    "Ramp Time", "Soak Time", "Reflow Time", "Hold Time"
};
int profileCreateMin[7] = { 60, 80, 140, 30, 30, 20, 5 };
int profileCreateMax[7] = { 200, 220, 280, 300, 300, 180, 120 };

// ===================== Default Recipes =====================
static const ReflowRecipe defaultRecipes[RECIPE_COUNT_DEFAULT] = {
    {RECIPE_REFLOW, "Sn63/Pb37",     100, 150, 220, 90,  90,  30,  30,  0, 0, 0},
    {RECIPE_REFLOW, "SAC305",        150, 200, 245, 90,  90,  30,  30,  0, 0, 0},
    {RECIPE_REFLOW, "Bi58/Sn42",     100, 140, 170, 90,  90,  30,  30,  0, 0, 0}
};

// ===================== Default Per-Zone Gains =====================
static const PidGains defaultZoneGains[NUM_ZONES] = {
    {2.4f, 0.025f, 0.96f},
    {1.6f, 0.075f, 0.6f},
    {2.0f, 0.05f,  1.56f},
    {1.2f, 0.015f, 2.16f},
    {0.2f, 0.0f,   0.12f}
};

static const PidGains defaultCoolingZoneGains[NUM_ZONES] = {
    {0.5f, 0.005f, 0.2f},
    {0.5f, 0.005f, 0.2f},
    {1.0f, 0.01f,  0.5f},
    {1.5f, 0.02f,  0.8f},
    {2.0f, 0.03f,  1.0f}
};

// ===================== NVS Functions =====================
void saveRecipeToNvs(int index) {
    prefs.begin(NVS_NAMESPACE, false);
    char key[20];

    snprintf(key, sizeof(key), "r%d_name", index);
    prefs.putString(key, recipes[index].name);
    snprintf(key, sizeof(key), "r%d_pre", index);
    prefs.putInt(key, recipes[index].preheatTemp);
    snprintf(key, sizeof(key), "r%d_soak", index);
    prefs.putInt(key, recipes[index].soakTemp);
    snprintf(key, sizeof(key), "r%d_peak", index);
    prefs.putInt(key, recipes[index].peakTemp);
    snprintf(key, sizeof(key), "r%d_ramp", index);
    prefs.putInt(key, recipes[index].preheatRampTime);
    snprintf(key, sizeof(key), "r%d_soakT", index);
    prefs.putInt(key, recipes[index].soakTime);
    snprintf(key, sizeof(key), "r%d_refl", index);
    prefs.putInt(key, recipes[index].reflowTime);
    snprintf(key, sizeof(key), "r%d_hold", index);
    prefs.putInt(key, recipes[index].peakHoldTime);
    snprintf(key, sizeof(key), "r%d_cool", index);
    prefs.putInt(key, recipes[index].cooldownTime);

    for (int z = 0; z < NUM_ZONES; z++) {
        snprintf(key, sizeof(key), "r%d_z%d_kp", index, z);
        prefs.putFloat(key, recipePidGains[index][z].Kp);
        snprintf(key, sizeof(key), "r%d_z%d_ki", index, z);
        prefs.putFloat(key, recipePidGains[index][z].Ki);
        snprintf(key, sizeof(key), "r%d_z%d_kd", index, z);
        prefs.putFloat(key, recipePidGains[index][z].Kd);

        snprintf(key, sizeof(key), "r%d_z%d_ckp", index, z);
        prefs.putFloat(key, recipeCoolingGains[index][z].Kp);
        snprintf(key, sizeof(key), "r%d_z%d_cki", index, z);
        prefs.putFloat(key, recipeCoolingGains[index][z].Ki);
        snprintf(key, sizeof(key), "r%d_z%d_ckd", index, z);
        prefs.putFloat(key, recipeCoolingGains[index][z].Kd);
    }

    prefs.end();
}

void loadRecipeFromNvs(int index) {
    prefs.begin(NVS_NAMESPACE, true);
    char key[20];

    snprintf(key, sizeof(key), "r%d_name", index);
    String name = prefs.getString(key, "Unknown");
    strncpy(recipes[index].name, name.c_str(), sizeof(recipes[index].name) - 1);
    recipes[index].name[sizeof(recipes[index].name) - 1] = '\0';

    snprintf(key, sizeof(key), "r%d_pre", index);
    recipes[index].preheatTemp = prefs.getInt(key, 100);
    snprintf(key, sizeof(key), "r%d_soak", index);
    recipes[index].soakTemp = prefs.getInt(key, 150);
    snprintf(key, sizeof(key), "r%d_peak", index);
    recipes[index].peakTemp = prefs.getInt(key, 220);
    snprintf(key, sizeof(key), "r%d_ramp", index);
    recipes[index].preheatRampTime = prefs.getInt(key, 90);
    snprintf(key, sizeof(key), "r%d_soakT", index);
    recipes[index].soakTime = prefs.getInt(key, 90);
    snprintf(key, sizeof(key), "r%d_refl", index);
    recipes[index].reflowTime = prefs.getInt(key, 40);
    snprintf(key, sizeof(key), "r%d_hold", index);
    recipes[index].peakHoldTime = prefs.getInt(key, 20);
    snprintf(key, sizeof(key), "r%d_cool", index);
    recipes[index].cooldownTime = prefs.getInt(key, 60);
    recipes[index].type = RECIPE_REFLOW;
    recipes[index].bakeTemp = 0;
    recipes[index].bakeDuration = 0;

    for (int z = 0; z < NUM_ZONES; z++) {
        snprintf(key, sizeof(key), "r%d_z%d_kp", index, z);
        recipePidGains[index][z].Kp = prefs.getFloat(key, defaultZoneGains[z].Kp);
        snprintf(key, sizeof(key), "r%d_z%d_ki", index, z);
        recipePidGains[index][z].Ki = prefs.getFloat(key, defaultZoneGains[z].Ki);
        snprintf(key, sizeof(key), "r%d_z%d_kd", index, z);
        recipePidGains[index][z].Kd = prefs.getFloat(key, defaultZoneGains[z].Kd);

        snprintf(key, sizeof(key), "r%d_z%d_ckp", index, z);
        recipeCoolingGains[index][z].Kp = prefs.getFloat(key, defaultCoolingZoneGains[z].Kp);
        snprintf(key, sizeof(key), "r%d_z%d_cki", index, z);
        recipeCoolingGains[index][z].Ki = prefs.getFloat(key, defaultCoolingZoneGains[z].Ki);
        snprintf(key, sizeof(key), "r%d_z%d_ckd", index, z);
        recipeCoolingGains[index][z].Kd = prefs.getFloat(key, defaultCoolingZoneGains[z].Kd);
    }

    prefs.end();
}

void savePidGainsForRecipeZone(int recipeIdx, int zoneIdx, float kp, float ki, float kd) {
    if (recipeIdx < 0 || recipeIdx >= storedRecipeCount) return;
    if (zoneIdx < 0 || zoneIdx >= NUM_ZONES) return;

    recipePidGains[recipeIdx][zoneIdx].Kp = kp;
    recipePidGains[recipeIdx][zoneIdx].Ki = ki;
    recipePidGains[recipeIdx][zoneIdx].Kd = kd;

    prefs.begin(NVS_NAMESPACE, false);
    char key[20];
    snprintf(key, sizeof(key), "r%d_z%d_kp", recipeIdx, zoneIdx);
    prefs.putFloat(key, kp);
    snprintf(key, sizeof(key), "r%d_z%d_ki", recipeIdx, zoneIdx);
    prefs.putFloat(key, ki);
    snprintf(key, sizeof(key), "r%d_z%d_kd", recipeIdx, zoneIdx);
    prefs.putFloat(key, kd);
    prefs.end();
}

void saveCoolingGainsForRecipeZone(int recipeIdx, int zoneIdx, float kp, float ki, float kd) {
    if (recipeIdx < 0 || recipeIdx >= storedRecipeCount) return;
    if (zoneIdx < 0 || zoneIdx >= NUM_ZONES) return;

    recipeCoolingGains[recipeIdx][zoneIdx].Kp = kp;
    recipeCoolingGains[recipeIdx][zoneIdx].Ki = ki;
    recipeCoolingGains[recipeIdx][zoneIdx].Kd = kd;

    prefs.begin(NVS_NAMESPACE, false);
    char key[20];
    snprintf(key, sizeof(key), "r%d_z%d_ckp", recipeIdx, zoneIdx);
    prefs.putFloat(key, kp);
    snprintf(key, sizeof(key), "r%d_z%d_cki", recipeIdx, zoneIdx);
    prefs.putFloat(key, ki);
    snprintf(key, sizeof(key), "r%d_z%d_ckd", recipeIdx, zoneIdx);
    prefs.putFloat(key, kd);
    prefs.end();
}

void saveGlobalPlantModel() {
    prefs.begin(NVS_NAMESPACE, false);
    for (int z = 0; z < NUM_ZONES; z++) {
        char key[12];
        snprintf(key, sizeof(key), "phr%d", z);
        prefs.putFloat(key, bresenhamPID.getHeatingRate(z));
        snprintf(key, sizeof(key), "pcr%d", z);
        prefs.putFloat(key, bresenhamPID.getCoolingRate(z));
    }
    prefs.putFloat("pdt", bresenhamPID.getDeadtime(0));
    prefs.end();
}

void loadGlobalPlantModel() {
    prefs.begin(NVS_NAMESPACE, true);
    float deadtime = prefs.getFloat("pdt", 2.0f);
    for (int z = 0; z < NUM_ZONES; z++) {
        char key[12];
        snprintf(key, sizeof(key), "phr%d", z);
        bresenhamPID.setHeatingRate(z, prefs.getFloat(key, 0.0f));
        snprintf(key, sizeof(key), "pcr%d", z);
        bresenhamPID.setCoolingRate(z, prefs.getFloat(key, 0.0f));
        bresenhamPID.setDeadtime(z, deadtime);
    }
    prefs.end();
}

void saveCalibrationToNvs() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putFloat("tc1Off", tc1Offset);
    prefs.putFloat("tc2Off", tc2Offset);
    prefs.end();
    tempReader.setCalibrationOffset(0, tc1Offset);
    tempReader.setCalibrationOffset(1, tc2Offset);
}

void addNewRecipeToNvs(const ReflowRecipe &recipe) {
    int newIndex = storedRecipeCount;
    recipes[newIndex] = recipe;

    for (int z = 0; z < NUM_ZONES; z++) {
        recipePidGains[newIndex][z] = defaultZoneGains[z];
        recipeCoolingGains[newIndex][z] = defaultCoolingZoneGains[z];
    }

    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("recipeCount", newIndex + 1);
    prefs.end();

    saveRecipeToNvs(newIndex);
    storedRecipeCount = newIndex + 1;
}

void initNvs() {
    prefs.begin(NVS_NAMESPACE, false);

    storedRecipeCount = prefs.getInt("recipeCount", 0);
    settingsMaxBakeMinutes = prefs.getInt("maxBakeMin", BAKE_DURATION_MAX_DEFAULT);
    useFahrenheit = prefs.getUChar("useF", 0);
    tc1Offset = prefs.getFloat("tc1Off", 0.0f);
    tc2Offset = prefs.getFloat("tc2Off", 0.0f);

    if (storedRecipeCount == 0) {
        for (int i = 0; i < RECIPE_COUNT_DEFAULT; i++) {
            recipes[i] = defaultRecipes[i];
            for (int z = 0; z < NUM_ZONES; z++) {
                recipePidGains[i][z] = defaultZoneGains[z];
                recipeCoolingGains[i][z] = defaultCoolingZoneGains[z];
            }
            saveRecipeToNvs(i);
        }
        storedRecipeCount = RECIPE_COUNT_DEFAULT;
        prefs.putInt("recipeCount", storedRecipeCount);
        prefs.putInt("maxBakeMin", BAKE_DURATION_MAX_DEFAULT);
        prefs.putUChar("useF", 0);
        prefs.putFloat("tc1Off", 0.0f);
        prefs.putFloat("tc2Off", 0.0f);
    } else {
        for (int i = 0; i < storedRecipeCount; i++) {
            loadRecipeFromNvs(i);
        }
    }

    loadGlobalPlantModel();
    tempReader.setCalibrationOffset(0, tc1Offset);
    tempReader.setCalibrationOffset(1, tc2Offset);

    prefs.end();
    Serial.printf("NVS loaded: %d recipes, maxBake=%d, tc1Off=%.1f tc2Off=%.1f\n",
                  storedRecipeCount, settingsMaxBakeMinutes, tc1Offset, tc2Offset);
}

// ===================== Profile Stage Label =====================
static const char* stageLabel(ProfileStage s) {
    switch (s) {
        case STAGE_PREHEAT:     return "PRE";
        case STAGE_SOAK:        return "SOAK";
        case STAGE_REFLOW_RAMP: return "RAMP";
        case STAGE_REFLOW_PEAK: return "PEAK";
        case STAGE_COOLDOWN:    return "COOL";
        default:                return "IDLE";
    }
}

// ===================== Safety Check =====================
void checkSafety(const TemperatureData &tempData) {
    if (tempData.sensor1Fault || tempData.sensor2Fault) {
        currentSystemState = STATE_ERROR;
        currentErrorCode = 1;
        bresenhamPID.emergencyStop();
        return;
    }

    if (tempData.temp1 > OVER_TEMP_LIMIT || tempData.temp2 > OVER_TEMP_LIMIT) {
        currentSystemState = STATE_ERROR;
        currentErrorCode = 2;
        bresenhamPID.emergencyStop();
        return;
    }

    if (tempData.spatialDelta > MAX_SPATIAL_DELTA) {
        currentSystemState = STATE_ERROR;
        currentErrorCode = 3;
        bresenhamPID.emergencyStop();
        return;
    }
}

// ===================== System Fan Control =====================
void updateSystemFan(float avgTemp) {
    float duty = ((avgTemp - SYS_FAN_TEMP_MIN) / (SYS_FAN_TEMP_MAX - SYS_FAN_TEMP_MIN)) * 255.0f;
    duty = constrain(duty, 0.0f, 255.0f);
    ledcWrite(SYS_FAN_PWM_CHAN, (uint32_t)duty);
}

// ===================== Core 0: Control Loop =====================
void core0ControlLoop(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    ProfileStage prevStage = STAGE_COMPLETE;
    float lastAvgTemp = AMBIENT_TEMP;

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS((TickType_t)bresenhamPID.getWindowMs()));

        if (!bresenhamPID.isAdcTriggered()) {
            continue;
        }
        bresenhamPID.clearAdcTrigger();

        TemperatureData tempData = tempReader.readSensors();

        float ts = bresenhamPID.getTimeStepS();
        float dTempDt = (tempData.avgTemp - lastAvgTemp) / ts;
        lastAvgTemp = tempData.avgTemp;

        updateSystemFan(tempData.avgTemp);

        if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            SystemState state = currentSystemState;

            if (state == STATE_RUNNING) {
                profileTimerSeconds += 2;
                if (profileTimerSeconds > profileEngine.getTotalDuration(activeRecipe)) {
                    currentSystemState = STATE_COOLING;
                    xSemaphoreGive(recipeMutex);
                    continue;
                }

                int targetTemp = profileEngine.calculateTargetTemp(profileTimerSeconds, activeRecipe);
                ProfileStage stage = profileEngine.getCurrentStage(profileTimerSeconds, activeRecipe);
                float dTargetDt = profileEngine.getTargetRampRate(profileTimerSeconds, activeRecipe);

                if (stage != prevStage && prevStage < NUM_ZONES) {
                    float curKp = bresenhamPID.getKp();
                    float curKi = bresenhamPID.getKi();
                    float curKd = bresenhamPID.getKd();
                    savePidGainsForRecipeZone(lastActiveRecipeIndex, (int)prevStage, curKp, curKi, curKd);

                    float curCkp = bresenhamPID.getCoolingKp();
                    float curCki = bresenhamPID.getCoolingKi();
                    float curCkd = bresenhamPID.getCoolingKd();
                    saveCoolingGainsForRecipeZone(lastActiveRecipeIndex, (int)prevStage, curCkp, curCki, curCkd);

                    float zoneKp = recipePidGains[lastActiveRecipeIndex][(int)stage].Kp;
                    float zoneKi = recipePidGains[lastActiveRecipeIndex][(int)stage].Ki;
                    float zoneKd = recipePidGains[lastActiveRecipeIndex][(int)stage].Kd;
                    bresenhamPID.setAIGains(zoneKp, zoneKi, zoneKd);
                    aiTuner.setBaseGains(zoneKp, zoneKi, zoneKd);

                    float cZoneKp = recipeCoolingGains[lastActiveRecipeIndex][(int)stage].Kp;
                    float cZoneKi = recipeCoolingGains[lastActiveRecipeIndex][(int)stage].Ki;
                    float cZoneKd = recipeCoolingGains[lastActiveRecipeIndex][(int)stage].Kd;
                    bresenhamPID.setCoolingGains(cZoneKp, cZoneKi, cZoneKd);
                    aiTuner.setCoolingBaseGains(cZoneKp, cZoneKi, cZoneKd);
                }
                prevStage = stage;

                if (stage == STAGE_COOLDOWN) {
                    bresenhamPID.setOutput(0);
                    bresenhamPID.setAIGains(0, 0, 0);

                    float ckp = bresenhamPID.getCoolingKp();
                    float cki = bresenhamPID.getCoolingKi();
                    float ckd = bresenhamPID.getCoolingKd();
                    aiTuner.runCoolingInference(tempData.avgTemp, tempData.spatialDelta,
                                                 targetTemp, stage, &ckp, &cki, &ckd);
                    bresenhamPID.setCoolingGains(ckp, cki, ckd);
                    bresenhamPID.runCoolingPID(targetTemp, tempData.avgTemp,
                                               dTargetDt, dTempDt, stage);
                } else {
                    float hotError = tempData.avgTemp - (float)targetTemp;
                    if (hotError > 10.0f) {
                        bresenhamPID.setOutput(0);
                        float ckp = 1.5f, cki = 0.02f, ckd = 0.8f;
                        aiTuner.runCoolingInference(tempData.avgTemp, tempData.spatialDelta,
                                                     targetTemp, stage, &ckp, &cki, &ckd);
                        bresenhamPID.setCoolingGains(ckp, cki, ckd);
                        bresenhamPID.runCoolingPID(targetTemp, tempData.avgTemp,
                                                   dTargetDt, dTempDt, stage);
                    } else {
                        bresenhamPID.setCoolingOutput(0);

                        float kp = bresenhamPID.getKp();
                        float ki = bresenhamPID.getKi();
                        float kd = bresenhamPID.getKd();
                        aiTuner.runInference(tempData.avgTemp, tempData.spatialDelta,
                                             targetTemp, stage, &kp, &ki, &kd);
                        bresenhamPID.setAIGains(kp, ki, kd);
                        bresenhamPID.runPIDLoop(targetTemp, tempData.avgTemp,
                                                tempData.temp1, tempData.temp2,
                                                dTargetDt, dTempDt, stage);
                    }
                }

                ThermalTelemetry telemetry;
                telemetry.tempTC1 = tempData.temp1;
                telemetry.tempTC2 = tempData.temp2;
                telemetry.avgTemp = tempData.avgTemp;
                telemetry.spatialDelta = tempData.spatialDelta;
                telemetry.targetTemp = targetTemp;
                telemetry.currentSeconds = profileTimerSeconds;
                telemetry.bresenhamDuty = bresenhamPID.getOutput();
                telemetry.coolingOutput = bresenhamPID.getCoolingOutput();
                telemetry.currentStage = stage;
                telemetry.systemState = state;
                telemetry.errorCode = 0;

                xQueueOverwrite(telemetryQueue, &telemetry);

            } else if (state == STATE_COOLING) {
                profileTimerSeconds += 2;

                ProfileStage stage = profileEngine.getCurrentStage(profileTimerSeconds, activeRecipe);
                int targetTemp = profileEngine.calculateTargetTemp(profileTimerSeconds, activeRecipe);
                float dTargetDt = profileEngine.getTargetRampRate(profileTimerSeconds, activeRecipe);

                if (stage != prevStage && prevStage < NUM_ZONES) {
                    float curKp = bresenhamPID.getKp();
                    float curKi = bresenhamPID.getKi();
                    float curKd = bresenhamPID.getKd();
                    savePidGainsForRecipeZone(lastActiveRecipeIndex, (int)prevStage, curKp, curKi, curKd);

                    float curCkp = bresenhamPID.getCoolingKp();
                    float curCki = bresenhamPID.getCoolingKi();
                    float curCkd = bresenhamPID.getCoolingKd();
                    saveCoolingGainsForRecipeZone(lastActiveRecipeIndex, (int)prevStage, curCkp, curCki, curCkd);
                }
                prevStage = stage;

                bresenhamPID.setOutput(0);
                bresenhamPID.setAIGains(0, 0, 0);

                float ckp = bresenhamPID.getCoolingKp();
                float cki = bresenhamPID.getCoolingKi();
                float ckd = bresenhamPID.getCoolingKd();
                aiTuner.runCoolingInference(tempData.avgTemp, tempData.spatialDelta,
                                             targetTemp, stage, &ckp, &cki, &ckd);
                bresenhamPID.setCoolingGains(ckp, cki, ckd);
                bresenhamPID.runCoolingPID(targetTemp, tempData.avgTemp,
                                           dTargetDt, dTempDt, stage);

                if (tempData.avgTemp < 50.0f) {
                    currentSystemState = STATE_IDLE;
                    profileTimerSeconds = 0;
                    bresenhamPID.reset();

                    saveCoolingGainsForRecipeZone(lastActiveRecipeIndex, NUM_ZONES - 1,
                                                  bresenhamPID.getCoolingKp(),
                                                  bresenhamPID.getCoolingKi(),
                                                  bresenhamPID.getCoolingKd());

                    xSemaphoreGive(recipeMutex);
                    continue;
                }

                ThermalTelemetry telemetry;
                telemetry.tempTC1 = tempData.temp1;
                telemetry.tempTC2 = tempData.temp2;
                telemetry.avgTemp = tempData.avgTemp;
                telemetry.spatialDelta = tempData.spatialDelta;
                telemetry.targetTemp = targetTemp;
                telemetry.currentSeconds = profileTimerSeconds;
                telemetry.bresenhamDuty = 0;
                telemetry.coolingOutput = bresenhamPID.getCoolingOutput();
                telemetry.currentStage = stage;
                telemetry.systemState = state;
                telemetry.errorCode = 0;

                xQueueOverwrite(telemetryQueue, &telemetry);

            } else if (state == STATE_BAKE_RUNNING) {
                profileTimerSeconds += 2;
                if (bakeTotalSeconds > 0 && profileTimerSeconds >= bakeTotalSeconds) {
                    currentSystemState = STATE_BAKE_COMPLETE;
                    xSemaphoreGive(recipeMutex);
                    continue;
                }

                int targetTemp = profileEngine.calculateTargetTemp(profileTimerSeconds, activeRecipe);
                float dTargetDt = 0.0f;

                float hotError = tempData.avgTemp - (float)targetTemp;
                if (hotError > 10.0f) {
                    bresenhamPID.setOutput(0);
                    float ckp = 1.5f, cki = 0.02f, ckd = 0.8f;
                    aiTuner.runCoolingInference(tempData.avgTemp, tempData.spatialDelta,
                                                 targetTemp, STAGE_SOAK, &ckp, &cki, &ckd);
                    bresenhamPID.setCoolingGains(ckp, cki, ckd);
                    bresenhamPID.runCoolingPID(targetTemp, tempData.avgTemp,
                                               dTargetDt, dTempDt, STAGE_SOAK);
                } else {
                    bresenhamPID.setCoolingOutput(0);
                    float kp = bresenhamPID.getKp();
                    float ki = bresenhamPID.getKi();
                    float kd = bresenhamPID.getKd();
                    aiTuner.runInference(tempData.avgTemp, tempData.spatialDelta,
                                         targetTemp, STAGE_SOAK, &kp, &ki, &kd);
                    bresenhamPID.setAIGains(kp, ki, kd);
                    bresenhamPID.runPIDLoop(targetTemp, tempData.avgTemp,
                                            tempData.temp1, tempData.temp2,
                                            dTargetDt, dTempDt, STAGE_SOAK);
                }

                ThermalTelemetry telemetry;
                telemetry.tempTC1 = tempData.temp1;
                telemetry.tempTC2 = tempData.temp2;
                telemetry.avgTemp = tempData.avgTemp;
                telemetry.spatialDelta = tempData.spatialDelta;
                telemetry.targetTemp = targetTemp;
                telemetry.currentSeconds = profileTimerSeconds;
                telemetry.bresenhamDuty = bresenhamPID.getOutput();
                telemetry.coolingOutput = bresenhamPID.getCoolingOutput();
                telemetry.currentStage = STAGE_SOAK;
                telemetry.systemState = state;
                telemetry.errorCode = 0;

                xQueueOverwrite(telemetryQueue, &telemetry);

            } else if (state == STATE_CAL_RUNNING) {
                static bool calDeadtimeResetDone = false;
                if (!calDeadtimeResetDone) {
                    bresenhamPID.resetDeadtimeDetection();
                    calDeadtimeResetDone = true;
                }

                profileTimerSeconds += 2;
                int preheatT = activeRecipe.preheatTemp;
                int soakT = activeRecipe.soakTemp;
                int peakT = activeRecipe.peakTemp;
                float avg = tempData.avgTemp;

                // Map calibration phase to profile stage for plant model measurement
                static const ProfileStage calStageMap[] = {
                    STAGE_PREHEAT, STAGE_SOAK, STAGE_REFLOW_RAMP,
                    STAGE_REFLOW_PEAK, STAGE_COOLDOWN
                };
                ProfileStage calStage = calStageMap[min(calPhase, 4)];

                if (calPhase == 0 && avg >= preheatT) {
                    calPreheatTime = profileTimerSeconds;
                    calPhase = 1;
                } else if (calPhase == 1 && avg >= soakT) {
                    calSoakTime = profileTimerSeconds - calPreheatTime;
                    calPhase = 2;
                } else if (calPhase == 2 && avg >= peakT) {
                    calReflowTime = profileTimerSeconds - calPreheatTime - calSoakTime;
                    calPeakReachedSec = profileTimerSeconds;
                    calPhase = 3;
                } else if (calPhase == 3 && (profileTimerSeconds - calPeakReachedSec) >= CAL_PEAK_HOLD_S) {
                    calPhase = 4;
                }

                if (calPhase < 4) {
                    bresenhamPID.setCoolingOutput(0);
                    float kp = 3.0f, ki = 0.1f, kd = 1.5f;
                    bresenhamPID.setAIGains(kp, ki, kd);
                    int target = (calPhase == 0) ? preheatT : (calPhase == 1) ? soakT : peakT;
                    bresenhamPID.runPIDLoop(target, avg, tempData.temp1, tempData.temp2,
                                            0.0f, dTempDt, calStage);
                    // Measure heating rate during active phases (heater at 100%)
                    bresenhamPID.measureRampRate(dTempDt, bresenhamPID.getOutput(), calStage);
                    bresenhamPID.measureDeadtime(bresenhamPID.getOutput(), dTempDt, calStage);
                } else if (calPhase == 4) {
                    bresenhamPID.setOutput(0);
                    bresenhamPID.setAIGains(0, 0, 0);
                    float ckp = 2.0f, cki = 0.03f, ckd = 1.0f;
                    bresenhamPID.setCoolingGains(ckp, cki, ckd);
                    bresenhamPID.runCoolingPID(AMBIENT_TEMP, avg,
                                               0.0f, dTempDt, calStage);
                    // Measure natural cooling rate
                    bresenhamPID.measureRampRate(dTempDt, 0, calStage);
                    // Track time from peak to CAL_COOLDOWN_TEMP for profile total time
                    if (calCooldownTime == 0 && avg <= (float)CAL_COOLDOWN_TEMP) {
                        calCooldownTime = profileTimerSeconds - calPeakReachedSec - CAL_PEAK_HOLD_S;
                        if (calCooldownTime < 10) calCooldownTime = 10;
                    }
                    if (avg < 50.0f) {
                        int newRamp = max(30, (int)(calPreheatTime * CAL_RUN_MARGIN));
                        int newSoak = max(30, (int)(calSoakTime * CAL_RUN_MARGIN));
                        int newReflow = max(20, (int)(calReflowTime * CAL_RUN_MARGIN));
                        int newHold = 30;
                        int newCooldown = max(CAL_COOLDOWN_MIN, (int)(calCooldownTime * CAL_RUN_MARGIN));

                        if (calTargetRecipeIndex >= 0 && calTargetRecipeIndex < storedRecipeCount) {
                            recipes[calTargetRecipeIndex].preheatRampTime = newRamp;
                            recipes[calTargetRecipeIndex].soakTime = newSoak;
                            recipes[calTargetRecipeIndex].reflowTime = newReflow;
                            recipes[calTargetRecipeIndex].peakHoldTime = newHold;
                            recipes[calTargetRecipeIndex].cooldownTime = newCooldown;
                            saveRecipeToNvs(calTargetRecipeIndex);
                        }

                        // Save measured plant model to global NVS
                        saveGlobalPlantModel();
                        Serial.println("Plant model calibrated and saved.");

                        currentSystemState = STATE_CAL_COMPLETE;
                        profileTimerSeconds = 0;
                        bresenhamPID.reset();
                        calDeadtimeResetDone = false;
                        xSemaphoreGive(recipeMutex);
                        continue;
                    }
                }

                int calTarget = (calPhase < 4) ? ((calPhase == 0) ? preheatT : (calPhase == 1) ? soakT : peakT) : (int)AMBIENT_TEMP;
                ThermalTelemetry telemetry;
                telemetry.tempTC1 = tempData.temp1;
                telemetry.tempTC2 = tempData.temp2;
                telemetry.avgTemp = avg;
                telemetry.spatialDelta = tempData.spatialDelta;
                telemetry.targetTemp = calTarget;
                telemetry.currentSeconds = profileTimerSeconds;
                telemetry.bresenhamDuty = bresenhamPID.getOutput();
                telemetry.coolingOutput = bresenhamPID.getCoolingOutput();
                telemetry.currentStage = calStage;
                telemetry.systemState = state;
                telemetry.errorCode = 0;

                xQueueOverwrite(telemetryQueue, &telemetry);

            } else if (state == STATE_ESTOP || state == STATE_ERROR) {
                bresenhamPID.emergencyStop();
            } else {
                profileTimerSeconds = 0;
                bresenhamPID.reset();
                tempReader.resetFilter();
                prevStage = STAGE_COMPLETE;
                lastAvgTemp = AMBIENT_TEMP;
            }

            xSemaphoreGive(recipeMutex);
        }

        checkSafety(tempData);
    }
}

// ===================== Core 1: UI Task =====================
void core1UITask(void *pvParameters) {
    ThermalTelemetry uiData;
    MenuState menuState = MENU_MAIN;
    bool splashShown = false;
    bool bakeRunningCompletePlayed = false;

    for (;;) {
        if (!splashShown) {
            display.renderSplash();
            splashShown = true;
            vTaskDelay(pdMS_TO_TICKS(1500));
        }

        SystemState state;
        if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            state = currentSystemState;
            xSemaphoreGive(recipeMutex);
        } else {
            state = STATE_IDLE;
        }

        switch (state) {
            case STATE_IDLE: {
                if (menuState != MENU_MAIN && menuState != MENU_RECIPE_SELECT
                    && menuState != MENU_PROFILE_CREATE
                    && menuState != MENU_BAKE_SETUP
                    && menuState != MENU_SETTINGS
                    && menuState != MENU_CALIBRATION
                    && menuState != MENU_CALIBRATION_GAINS) {
                    menuState = MENU_MAIN;
                }

                if (menuState == MENU_PROFILE_CREATE) {
                    display.renderProfileCreate(
                        profileCreateStep, PROFILE_CREATE_STEPS,
                        profileCreateLabels[profileCreateStep],
                        profileCreateValues[profileCreateStep],
                        profileCreateMin[profileCreateStep],
                        profileCreateMax[profileCreateStep],
                        profileCreateStep <= 2
                    );

                    if (buttonDebouncer.isPressed(buttonDebouncer.btnUp) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnRight)) {
                        profileCreateValues[profileCreateStep] += 5;
                        if (profileCreateValues[profileCreateStep] > profileCreateMax[profileCreateStep]) {
                            profileCreateValues[profileCreateStep] = profileCreateMin[profileCreateStep];
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnDown) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnLeft)) {
                        profileCreateValues[profileCreateStep] -= 5;
                        if (profileCreateValues[profileCreateStep] < profileCreateMin[profileCreateStep]) {
                            profileCreateValues[profileCreateStep] = profileCreateMax[profileCreateStep];
                        }
                    }
                    if (isSShortPress()) {
                        profileCreateStep++;
                        if (profileCreateStep >= PROFILE_CREATE_STEPS) {
                            char newName[16];
                            snprintf(newName, sizeof(newName), "Profile %d", storedRecipeCount + 1);

                            ReflowRecipe newRecipe;
                            newRecipe.type = RECIPE_REFLOW;
                            strncpy(newRecipe.name, newName, sizeof(newRecipe.name) - 1);
                            newRecipe.name[sizeof(newRecipe.name) - 1] = '\0';
                            newRecipe.preheatTemp = profileCreateValues[0];
                            newRecipe.soakTemp = profileCreateValues[1];
                            newRecipe.peakTemp = profileCreateValues[2];
                            newRecipe.preheatRampTime = profileCreateValues[3];
                            newRecipe.soakTime = profileCreateValues[4];
                            newRecipe.reflowTime = profileCreateValues[5];
                            newRecipe.peakHoldTime = profileCreateValues[6];
                            newRecipe.cooldownTime = 0;
                            newRecipe.bakeTemp = 0;
                            newRecipe.bakeDuration = 0;

                            addNewRecipeToNvs(newRecipe);

                            profileCreateActive = false;
                            profileCreateStep = 0;
                            menuState = MENU_RECIPE_SELECT;
                            selectedRecipeIndex = storedRecipeCount - 1;
                            buzzer.trigger(BUZZER_PHASE_TRANSITION);
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop) ||
                        isSLongPress()) {
                        profileCreateActive = false;
                        profileCreateStep = 0;
                        menuState = MENU_RECIPE_SELECT;
                    }
                    break;
                }

                if (menuState == MENU_BAKE_SETUP) {
                    display.renderBakeSetup(bakeTargetTemp, bakeTargetDurationMin, settingsMaxBakeMinutes, bakeEditingTemp);

                    if (buttonDebouncer.isPressed(buttonDebouncer.btnUp) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnRight)) {
                        if (bakeEditingTemp) {
                            bakeTargetTemp += 5;
                            if (bakeTargetTemp > BAKE_TEMP_MAX) bakeTargetTemp = BAKE_TEMP_MIN;
                        } else {
                            bakeTargetDurationMin += 1;
                            if (bakeTargetDurationMin > settingsMaxBakeMinutes) bakeTargetDurationMin = BAKE_DURATION_MIN;
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnDown) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnLeft)) {
                        if (bakeEditingTemp) {
                            bakeTargetTemp -= 5;
                            if (bakeTargetTemp < BAKE_TEMP_MIN) bakeTargetTemp = BAKE_TEMP_MAX;
                        } else {
                            bakeTargetDurationMin -= 1;
                            if (bakeTargetDurationMin < BAKE_DURATION_MIN) bakeTargetDurationMin = settingsMaxBakeMinutes;
                        }
                    }
                    if (isSShortPress()) {
                        bakeEditingTemp = !bakeEditingTemp;
                    }
                    if (isSLongPress()) {
                        if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            ReflowRecipe bakeRecipe;
                            bakeRecipe.type = RECIPE_BAKE;
                            strncpy(bakeRecipe.name, "Baking", sizeof(bakeRecipe.name) - 1);
                            bakeRecipe.name[sizeof(bakeRecipe.name) - 1] = '\0';
                            bakeRecipe.bakeTemp = bakeTargetTemp;
                            bakeRecipe.bakeDuration = bakeTargetDurationMin;
                            activeRecipe = bakeRecipe;

                            bakeTotalSeconds = bakeTargetDurationMin * 60;
                            profileTimerSeconds = 0;
                            bresenhamPID.reset();
                            tempReader.resetFilter();
                            bresenhamPID.setAIGains(1.6f, 0.075f, 0.6f);
                            bresenhamPID.setCoolingGains(0.5f, 0.005f, 0.2f);
                            aiTuner.setBaseGains(1.6f, 0.075f, 0.6f);
                            aiTuner.setCoolingBaseGains(0.5f, 0.005f, 0.2f);

                            currentSystemState = STATE_BAKE_RUNNING;
                            xSemaphoreGive(recipeMutex);
                            buzzer.trigger(BUZZER_PHASE_TRANSITION);
                            menuState = MENU_BAKE_RUNNING;
                            bakeRunningCompletePlayed = false;
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                        menuState = MENU_RECIPE_SELECT;
                    }
                    break;
                }

                if (menuState == MENU_SETTINGS) {
                    display.renderSettings(settingsMaxBakeMinutes, settingsSelectedRow, useFahrenheit);

                    if (buttonDebouncer.isPressed(buttonDebouncer.btnUp) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnRight)) {
                        if (settingsSelectedRow == 0) {
                            settingsMaxBakeMinutes += 5;
                            if (settingsMaxBakeMinutes > 300) settingsMaxBakeMinutes = 1;
                        } else {
                            useFahrenheit = !useFahrenheit;
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnDown) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnLeft)) {
                        if (settingsSelectedRow == 0) {
                            settingsMaxBakeMinutes -= 5;
                            if (settingsMaxBakeMinutes < 1) settingsMaxBakeMinutes = 300;
                        } else {
                            useFahrenheit = !useFahrenheit;
                        }
                    }
                    if (isSShortPress()) {
                        settingsSelectedRow = (settingsSelectedRow + 1) % 2;
                    }
                    if (isSLongPress()) {
                        prefs.begin(NVS_NAMESPACE, false);
                        prefs.putInt("maxBakeMin", settingsMaxBakeMinutes);
                        prefs.putUChar("useF", useFahrenheit ? 1 : 0);
                        prefs.end();
                        display.setUseFahrenheit(useFahrenheit);
                        menuState = MENU_RECIPE_SELECT;
                    }
                    break;
                }

                if (menuState == MENU_CALIBRATION) {
                    display.renderCalibration(calibrationSelectedItem, calibrationEditMode, tc1Offset, tc2Offset);

                    if (buttonDebouncer.isPressed(buttonDebouncer.btnUp) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnRight)) {
                        if (calibrationEditMode && calibrationSelectedItem < 2) {
                            float *off = (calibrationSelectedItem == 0) ? &tc1Offset : &tc2Offset;
                            *off += CAL_OFFSET_STEP;
                            if (*off > CAL_OFFSET_MAX) *off = CAL_OFFSET_MIN;
                        } else {
                            calibrationSelectedItem = (calibrationSelectedItem - 1 + 4) % 4;
                            calibrationEditMode = false;
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnDown) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnLeft)) {
                        if (calibrationEditMode && calibrationSelectedItem < 2) {
                            float *off = (calibrationSelectedItem == 0) ? &tc1Offset : &tc2Offset;
                            *off -= CAL_OFFSET_STEP;
                            if (*off < CAL_OFFSET_MIN) *off = CAL_OFFSET_MAX;
                        } else {
                            calibrationSelectedItem = (calibrationSelectedItem + 1) % 4;
                            calibrationEditMode = false;
                        }
                    }
                    if (isSShortPress()) {
                        if (calibrationSelectedItem == 2) {
                            calibrationViewZone = 0;
                            menuState = MENU_CALIBRATION_GAINS;
                        } else if (calibrationSelectedItem == 3) {
                            calTargetRecipeIndex = (selectedRecipeIndex >= 0 && selectedRecipeIndex < storedRecipeCount) ? selectedRecipeIndex : 0;
                            calPhase = 0;
                            calPreheatTime = 0;
                            calSoakTime = 0;
                            calReflowTime = 0;
                            calPeakReachedSec = 0;
                            menuState = MENU_CAL_RUN_CONFIRM;
                        } else {
                            calibrationEditMode = !calibrationEditMode;
                        }
                    }
                    if (isSLongPress()) {
                        saveCalibrationToNvs();
                        menuState = MENU_RECIPE_SELECT;
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                        tc1Offset = prefs.getFloat("tc1Off", 0.0f);
                        tc2Offset = prefs.getFloat("tc2Off", 0.0f);
                        calibrationEditMode = false;
                        menuState = MENU_RECIPE_SELECT;
                    }
                    break;
                }

                if (menuState == MENU_CALIBRATION_GAINS) {
                    int recipeIdx = (selectedRecipeIndex >= 0 && selectedRecipeIndex < storedRecipeCount)
                                    ? selectedRecipeIndex : 0;
                    PidGains hg = recipePidGains[recipeIdx][calibrationViewZone];
                    PidGains cg = recipeCoolingGains[recipeIdx][calibrationViewZone];
                    display.renderZoneGains(calibrationViewZone, hg, cg);

                    if (buttonDebouncer.isPressed(buttonDebouncer.btnUp) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnRight)) {
                        calibrationViewZone = (calibrationViewZone + 1) % NUM_ZONES;
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnDown) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnLeft)) {
                        calibrationViewZone = (calibrationViewZone - 1 + NUM_ZONES) % NUM_ZONES;
                    }
                    if (isSShortPress() ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                        menuState = MENU_CALIBRATION;
                    }
                    break;
                }

                if (menuState == MENU_CAL_RUN_CONFIRM) {
                    display.renderCalRunConfirm(recipes[calTargetRecipeIndex].name);

                    if (isSShortPress()) {
                        if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            activeRecipe = recipes[calTargetRecipeIndex];
                            profileTimerSeconds = 0;
                            calPhase = 0;
                            calPreheatTime = 0;
                            calSoakTime = 0;
                            calReflowTime = 0;
                            calPeakReachedSec = 0;
                            calCooldownTime = 0;
                            bresenhamPID.reset();
                            currentSystemState = STATE_CAL_RUNNING;
                            xSemaphoreGive(recipeMutex);
                            buzzer.trigger(BUZZER_PHASE_TRANSITION);
                            menuState = MENU_CAL_RUNNING;
                        }
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                        menuState = MENU_CALIBRATION;
                    }
                    break;
                }

                if (isSLongPress() && menuState == MENU_MAIN) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        activeRecipe = recipes[selectedRecipeIndex];
                        lastActiveRecipeIndex = selectedRecipeIndex;
                        profileTimerSeconds = 0;
                        previousControlStage = STAGE_COMPLETE;

                        float kp = recipePidGains[selectedRecipeIndex][0].Kp;
                        float ki = recipePidGains[selectedRecipeIndex][0].Ki;
                        float kd = recipePidGains[selectedRecipeIndex][0].Kd;
                        bresenhamPID.setAIGains(kp, ki, kd);
                        aiTuner.setBaseGains(kp, ki, kd);

                        float ckp = recipeCoolingGains[selectedRecipeIndex][0].Kp;
                        float cki = recipeCoolingGains[selectedRecipeIndex][0].Ki;
                        float ckd = recipeCoolingGains[selectedRecipeIndex][0].Kd;
                        bresenhamPID.setCoolingGains(ckp, cki, ckd);
                        aiTuner.setCoolingBaseGains(ckp, cki, ckd);

                        bresenhamPID.reset();
                        tempReader.resetFilter();
                        currentSystemState = STATE_RUNNING;
                        xSemaphoreGive(recipeMutex);
                        buzzer.trigger(BUZZER_PHASE_TRANSITION);
                        menuState = MENU_RUNNING;
                    }
                }

                if (isSShortPress()) {
                    if (menuState == MENU_MAIN) {
                        menuState = MENU_RECIPE_SELECT;
                    } else if (menuState == MENU_RECIPE_SELECT) {
                        int createOption = storedRecipeCount;
                        int bakeOption = storedRecipeCount + 1;
                        int settingsOption = storedRecipeCount + 2;
                        int calOption = storedRecipeCount + 3;

                        if (selectedRecipeIndex == createOption) {
                            profileCreateValues[0] = 150;
                            profileCreateValues[1] = 200;
                            profileCreateValues[2] = 245;
                            profileCreateValues[3] = 90;
                            profileCreateValues[4] = 90;
                            profileCreateValues[5] = 30;
                            profileCreateValues[6] = 30;
                            profileCreateStep = 0;
                            profileCreateActive = true;
                            menuState = MENU_PROFILE_CREATE;
                        } else if (selectedRecipeIndex == bakeOption) {
                            bakeTargetTemp = 100;
                            bakeTargetDurationMin = BAKE_DURATION_DEFAULT;
                            bakeEditingTemp = true;
                            menuState = MENU_BAKE_SETUP;
                        } else if (selectedRecipeIndex == settingsOption) {
                            settingsSelectedRow = 0;
                            menuState = MENU_SETTINGS;
                        } else if (selectedRecipeIndex == calOption) {
                            calibrationSelectedItem = 0;
                            calibrationEditMode = false;
                            menuState = MENU_CALIBRATION;
                        } else if (selectedRecipeIndex >= 0 && selectedRecipeIndex < storedRecipeCount) {
                            if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                                activeRecipe = recipes[selectedRecipeIndex];
                                lastActiveRecipeIndex = selectedRecipeIndex;
                                profileTimerSeconds = 0;
                                previousControlStage = STAGE_COMPLETE;

                                float kp = recipePidGains[selectedRecipeIndex][0].Kp;
                                float ki = recipePidGains[selectedRecipeIndex][0].Ki;
                                float kd = recipePidGains[selectedRecipeIndex][0].Kd;
                                bresenhamPID.setAIGains(kp, ki, kd);
                                aiTuner.setBaseGains(kp, ki, kd);

                                float ckp = recipeCoolingGains[selectedRecipeIndex][0].Kp;
                                float cki = recipeCoolingGains[selectedRecipeIndex][0].Ki;
                                float ckd = recipeCoolingGains[selectedRecipeIndex][0].Kd;
                                bresenhamPID.setCoolingGains(ckp, cki, ckd);
                                aiTuner.setCoolingBaseGains(ckp, cki, ckd);

                                bresenhamPID.reset();
                                tempReader.resetFilter();
                                currentSystemState = STATE_RUNNING;
                                xSemaphoreGive(recipeMutex);
                                buzzer.trigger(BUZZER_PHASE_TRANSITION);
                                menuState = MENU_RUNNING;
                            }
                        }
                    }
                }

                if (menuState == MENU_RECIPE_SELECT) {
                    int createOption = storedRecipeCount;
                    int bakeOption = storedRecipeCount + 1;
                    int settingsOption = storedRecipeCount + 2;
                    int calOption = storedRecipeCount + 3;
                    int totalOptions = storedRecipeCount + 4;

                    if (buttonDebouncer.isPressed(buttonDebouncer.btnUp) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnRight)) {
                        selectedRecipeIndex = (selectedRecipeIndex + 1) % totalOptions;
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnDown) ||
                        buttonDebouncer.isPressed(buttonDebouncer.btnLeft)) {
                        selectedRecipeIndex = (selectedRecipeIndex - 1 + totalOptions) % totalOptions;
                    }
                    if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                        menuState = MENU_MAIN;
                    }
                    display.renderRecipeSelect(selectedRecipeIndex, recipes, storedRecipeCount, true, true, true, true);
                } else {
                    display.renderMainMenu(selectedRecipeIndex);
                }
                break;
            }

            case STATE_RUNNING:
            case STATE_COOLING: {
                menuState = MENU_RUNNING;

                if (xQueueReceive(telemetryQueue, &uiData, pdMS_TO_TICKS(20)) == pdTRUE) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        display.renderLivePlot(uiData, activeRecipe);
                        xSemaphoreGive(recipeMutex);
                    }

                    static ProfileStage lastStage = STAGE_PREHEAT;
                    if (uiData.currentStage != lastStage) {
                        buzzer.trigger(BUZZER_PHASE_TRANSITION);
                        lastStage = uiData.currentStage;
                    }
                }

                if (state == STATE_COOLING) {
                    if (tempReader.readSensors().avgTemp < 55.0f) {
                        buzzer.trigger(BUZZER_CYCLE_COMPLETE);
                    }
                }

                if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_ESTOP;
                        bresenhamPID.emergencyStop();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.trigger(BUZZER_ESTOP);
                    menuState = MENU_ESTOP;
                }
                break;
            }

            case STATE_CAL_RUNNING: {
                menuState = MENU_CAL_RUNNING;
                if (xQueueReceive(telemetryQueue, &uiData, pdMS_TO_TICKS(20)) == pdTRUE) {
                    display.renderCalRunning(uiData.targetTemp, uiData.avgTemp,
                                             uiData.currentSeconds, (int)uiData.currentStage);
                }
                if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_ESTOP;
                        bresenhamPID.emergencyStop();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.trigger(BUZZER_ESTOP);
                    menuState = MENU_ESTOP;
                }
                break;
            }

            case STATE_BAKE_RUNNING: {
                if (xQueueReceive(telemetryQueue, &uiData, pdMS_TO_TICKS(20)) == pdTRUE) {
                    display.renderBakeRunning(uiData.tempTC1, uiData.tempTC2, uiData.targetTemp,
                                              uiData.currentSeconds, bakeTotalSeconds);
                }

                if (buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_ESTOP;
                        bresenhamPID.emergencyStop();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.trigger(BUZZER_ESTOP);
                    menuState = MENU_ESTOP;
                }
                break;
            }

            case STATE_BAKE_COMPLETE: {
                if (!bakeRunningCompletePlayed) {
                    buzzer.trigger(BUZZER_CYCLE_COMPLETE);
                    bakeRunningCompletePlayed = true;
                }
                display.renderBakeRunning(
                    uiData.tempTC1,
                    uiData.tempTC2,
                    uiData.targetTemp,
                    profileTimerSeconds > bakeTotalSeconds ? bakeTotalSeconds : profileTimerSeconds,
                    bakeTotalSeconds
                );
                if (isSShortPress() ||
                    buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_IDLE;
                        profileTimerSeconds = 0;
                        bresenhamPID.reset();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.stop();
                    menuState = MENU_MAIN;
                }
                break;
            }

            case STATE_CAL_COMPLETE: {
                if (calTargetRecipeIndex >= 0 && calTargetRecipeIndex < storedRecipeCount) {
                    const ReflowRecipe &r = recipes[calTargetRecipeIndex];
                    display.renderCalComplete(r.name, r.preheatRampTime, r.soakTime, r.reflowTime, r.peakHoldTime, r.cooldownTime);
                }
                if (isSShortPress() ||
                    buttonDebouncer.isPressed(buttonDebouncer.btnEStop)) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_IDLE;
                        profileTimerSeconds = 0;
                        bresenhamPID.reset();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.trigger(BUZZER_CYCLE_COMPLETE);
                    menuState = MENU_MAIN;
                }
                break;
            }

            case STATE_ESTOP: {
                display.renderEStopScreen();
                buzzer.update();

                if (isSShortPress()) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_IDLE;
                        profileTimerSeconds = 0;
                        bresenhamPID.reset();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.stop();
                    menuState = MENU_MAIN;
                }
                break;
            }

            case STATE_ERROR: {
                display.renderErrorScreen(currentErrorCode);
                buzzer.trigger(BUZZER_ERROR_SENSOR);
                buzzer.update();

                if (isSShortPress()) {
                    if (xSemaphoreTake(recipeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentSystemState = STATE_IDLE;
                        currentErrorCode = 0;
                        profileTimerSeconds = 0;
                        bresenhamPID.reset();
                        xSemaphoreGive(recipeMutex);
                    }
                    buzzer.stop();
                    menuState = MENU_MAIN;
                }
                break;
            }
        }

        buzzer.update();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ===================== Setup =====================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("Reflow Oven Controller v" FIRMWARE_VERSION);

    initNvs();

    bresenhamPID.begin();

    prefs.begin(NVS_NAMESPACE, false);
    float savedFreq = prefs.getFloat("lineFreq", 0.0f);
    float measuredFreq = bresenhamPID.getMeasuredFrequency();
    if (fabsf(measuredFreq - savedFreq) > 0.1f) {
        prefs.putFloat("lineFreq", measuredFreq);
        Serial.printf("Line frequency calibrated: %.1f Hz (saved)\n", measuredFreq);
    } else {
        Serial.printf("Line frequency: %.1f Hz (from NVS)\n", measuredFreq);
    }
    prefs.end();

    display.setMeasuredFrequency(bresenhamPID.getMeasuredFrequency());

    tempReader.begin();
    buttonDebouncer.begin();
    buzzer.begin();
    display.begin();
    display.setUseFahrenheit(useFahrenheit);

    ledcSetup(SYS_FAN_PWM_CHAN, SYS_FAN_PWM_FREQ, SYS_FAN_PWM_RES);
    ledcAttachPin(PIN_SYS_FAN_PWM, SYS_FAN_PWM_CHAN);

    aiTuner.begin();
    aiTuner.setBaseGains(2.0f, 0.05f, 1.2f);
    aiTuner.setCoolingBaseGains(1.0f, 0.01f, 0.5f);
    aiTuner.setStageTuningEnabled(false);

    telemetryQueue = xQueueCreate(2, sizeof(ThermalTelemetry));
    recipeMutex = xSemaphoreCreateMutex();

    if (telemetryQueue == NULL || recipeMutex == NULL) {
        Serial.println("FATAL: Failed to create queue/mutex");
        while (1) { vTaskDelay(1000); }
    }

    xTaskCreatePinnedToCore(
        core0ControlLoop,
        "Core0_Ctrl",
        4096,
        NULL,
        3,
        &core0TaskHandle,
        0
    );

    xTaskCreatePinnedToCore(
        core1UITask,
        "Core1_UI",
        8192,
        NULL,
        1,
        &core1TaskHandle,
        1
    );

    Serial.println("Tasks created. System ready.");
}

void loop() {
    vTaskDelete(NULL);
}
