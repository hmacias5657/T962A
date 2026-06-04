#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

#define NUM_ZONES 5

enum RecipeType : uint8_t {
    RECIPE_REFLOW = 0,
    RECIPE_BAKE
};

struct ReflowRecipe {
    RecipeType type;
    char name[16];
    int preheatTemp;
    int soakTemp;
    int peakTemp;
    int preheatRampTime;
    int soakTime;
    int reflowTime;
    int peakHoldTime;
    int cooldownTime;
    int bakeTemp;
    int bakeDuration;
};

struct PidGains {
    float Kp;
    float Ki;
    float Kd;
};

enum SystemState : uint8_t {
    STATE_IDLE = 0,
    STATE_RUNNING,
    STATE_COOLING,
    STATE_BAKE_RUNNING,
    STATE_BAKE_COMPLETE,
    STATE_CAL_RUNNING,
    STATE_CAL_COMPLETE,
    STATE_ESTOP,
    STATE_ERROR
};

enum ProfileStage : uint8_t {
    STAGE_PREHEAT = 0,
    STAGE_SOAK,
    STAGE_REFLOW_RAMP,
    STAGE_REFLOW_PEAK,
    STAGE_COOLDOWN,
    STAGE_COMPLETE
};

struct ThermalTelemetry {
    float tempTC1;
    float tempTC2;
    float avgTemp;
    float spatialDelta;
    int targetTemp;
    int currentSeconds;
    int bresenhamDuty;
    int coolingOutput;
    ProfileStage currentStage;
    SystemState systemState;
    uint8_t errorCode;
};

#endif
