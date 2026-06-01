#ifndef AI_TUNER_H
#define AI_TUNER_H

#include "SharedData.h"

class AITuner {
public:
    AITuner();

    void begin();

    // Heater gain scheduling
    void runInference(float avgTemp, float spatialDelta, int targetTemp,
                      ProfileStage stage, float *kp, float *ki, float *kd);
    void setBaseGains(float kp, float ki, float kd);
    void setLearningEnabled(bool enabled) { _learningEnabled = enabled; }
    void setStageTuningEnabled(bool enabled) { _stageTuningEnabled = enabled; }

    // Cooling gain scheduling
    void runCoolingInference(float avgTemp, float spatialDelta, int targetTemp,
                              ProfileStage stage, float *kp, float *ki, float *kd);
    void setCoolingBaseGains(float kp, float ki, float kd);

private:
    // Heater
    float _baseKp, _baseKi, _baseKd;
    float _prevSpatialDelta;
    bool _learningEnabled;
    bool _stageTuningEnabled;

    // Cooling
    float _coolingBaseKp, _coolingBaseKi, _coolingBaseKd;
    float _coolingPrevSpatialDelta;
    bool _coolingLearningEnabled;

    void applyStageTuning(ProfileStage stage, float *kp, float *ki, float *kd) const;
    void applySpatialDamping(float spatialDelta, float *kp, float *kd) const;
};

#endif
