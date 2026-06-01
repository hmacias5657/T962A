#include "AITuner.h"
#include <math.h>

AITuner::AITuner()
    : _baseKp(2.0f), _baseKi(0.05f), _baseKd(1.2f),
      _prevSpatialDelta(0.0f), _learningEnabled(true), _stageTuningEnabled(false),
      _coolingBaseKp(1.0f), _coolingBaseKi(0.01f), _coolingBaseKd(0.5f),
      _coolingPrevSpatialDelta(0.0f), _coolingLearningEnabled(true) {}

void AITuner::begin() {
    _stageTuningEnabled = false;
}

void AITuner::setBaseGains(float kp, float ki, float kd) {
    _baseKp = kp;
    _baseKi = ki;
    _baseKd = kd;
}

void AITuner::setCoolingBaseGains(float kp, float ki, float kd) {
    _coolingBaseKp = kp;
    _coolingBaseKi = ki;
    _coolingBaseKd = kd;
}

void AITuner::applyStageTuning(ProfileStage stage, float *kp, float *ki, float *kd) const {
    switch (stage) {
        case STAGE_PREHEAT:
            *kp *= 1.2f; *ki *= 0.5f; *kd *= 0.8f;
            break;
        case STAGE_SOAK:
            *kp *= 0.8f; *ki *= 1.5f; *kd *= 0.5f;
            break;
        case STAGE_REFLOW_RAMP:
            *kp *= 1.0f; *ki *= 1.0f; *kd *= 1.3f;
            break;
        case STAGE_REFLOW_PEAK:
            *kp *= 0.6f; *ki *= 0.3f; *kd *= 1.8f;
            break;
        case STAGE_COOLDOWN:
            *kp *= 0.1f; *ki *= 0.0f; *kd *= 0.1f;
            break;
        default:
            break;
    }
}

void AITuner::applySpatialDamping(float spatialDelta, float *kp, float *kd) const {
    if (spatialDelta > 20.0f) {
        float dampingFactor = 1.0f - ((spatialDelta - 20.0f) / 40.0f);
        dampingFactor = fmax(dampingFactor, 0.3f);
        *kp *= dampingFactor;
        *kd *= (1.0f + (1.0f - dampingFactor));
    }
}

void AITuner::runInference(float avgTemp, float spatialDelta, int targetTemp,
                           ProfileStage stage, float *kp, float *ki, float *kd) {
    *kp = _baseKp;
    *ki = _baseKi;
    *kd = _baseKd;

    if (_stageTuningEnabled) {
        applyStageTuning(stage, kp, ki, kd);
    }

    applySpatialDamping(spatialDelta, kp, kd);

    if (_learningEnabled) {
        float error = (float)targetTemp - avgTemp;
        float errorDerivative = error - _prevSpatialDelta;

        if (fabs(errorDerivative) > 5.0f && fabs(error) < 20.0f) {
            *ki *= 1.1f;
        }

        _prevSpatialDelta = spatialDelta;
    }
}

void AITuner::runCoolingInference(float avgTemp, float spatialDelta, int targetTemp,
                                   ProfileStage stage, float *kp, float *ki, float *kd) {
    *kp = _coolingBaseKp;
    *ki = _coolingBaseKi;
    *kd = _coolingBaseKd;

    if (spatialDelta > 15.0f) {
        float boost = 1.0f + ((spatialDelta - 15.0f) / 40.0f);
        boost = fmin(boost, 2.0f);
        *kp *= boost;
    }

    if (_coolingLearningEnabled) {
        float error = avgTemp - (float)targetTemp;
        float errorDerivative = error - _coolingPrevSpatialDelta;

        if (fabs(errorDerivative) > 3.0f && error > 5.0f) {
            *kp *= 1.05f;
        }

        _coolingPrevSpatialDelta = spatialDelta;
    }
}
