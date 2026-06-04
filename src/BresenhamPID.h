#ifndef BRESENHAM_PID_H
#define BRESENHAM_PID_H

#include <Arduino.h>
#include "Config.h"
#include "SharedData.h"

class BresenhamPID {
public:
    BresenhamPID();

    void begin();
    void IRAM_ATTR handleZeroCrossing();

    // Heater channel
    float getKp() const { return _Kp; }
    float getKi() const { return _Ki; }
    float getKd() const { return _Kd; }
    void setAIGains(float kp, float ki, float kd);
    void setOutput(int output) { _bresenhamOutput = constrain(output, 0, BRESENHAM_CYCLES); }
    int getOutput() const { return _bresenhamOutput; }
    void runPIDLoop(int targetTemp, float avgTemp, float temp1, float temp2,
                    float dTargetDt, float dTempDt, ProfileStage stage);

    // Cooling channel
    float getCoolingKp() const { return _coolingKp; }
    float getCoolingKi() const { return _coolingKi; }
    float getCoolingKd() const { return _coolingKd; }
    void setCoolingGains(float kp, float ki, float kd);
    void setCoolingOutput(int output) { _coolingOutput = constrain(output, 0, BRESENHAM_CYCLES); }
    int getCoolingOutput() const { return _coolingOutput; }
    void runCoolingPID(int targetTemp, float avgTemp);
    void runCoolingPID(int targetTemp, float avgTemp, float dTargetDt, float dTempDt, ProfileStage stage);

    // Common
    void reset();
    bool isAdcTriggered() const { return _triggerAdcRead; }
    void clearAdcTrigger() { _triggerAdcRead = false; }
    void emergencyStop();

    // Plant model — per-zone characteristics for feedforward
    // Measured once by calibration routine, stored globally in NVS
    float getHeatingRate(int zone) const;
    float getCoolingRate(int zone) const;
    float getDeadtime(int zone) const;
    void setHeatingRate(int zone, float rate);
    void setCoolingRate(int zone, float rate);
    void setDeadtime(int zone, float dt);

    // Called by calibration routine to measure plant characteristics
    void measureRampRate(float dTempDt, int output, ProfileStage stage);
    void measureDeadtime(int output, float dTempDt, ProfileStage stage);
    void resetDeadtimeDetection();

    // Line frequency calibration
    static const int CAL_CYCLES = 40;
    float getMeasuredFrequency() const { return _freqHz; }
    float getMeasuredHalfPeriodUs() const { return _halfPeriodUs; }
    float getWindowMs() const;
    float getTimeStepS() const;
    void setMeasuredFrequency(float freqHz);

private:
    static const int RING_BUF_SIZE = 20;
    volatile int _bresenhamOutput;
    volatile int _coolingOutput;
    volatile bool _triggerAdcRead;
    volatile uint16_t _zcCycleCount;

    // Heater gains
    float _Kp, _Ki, _Kd;
    float _integral, _lastError;
    bool _firstRun;

    // Cooling gains
    float _coolingKp, _coolingKi, _coolingKd;
    float _coolingIntegral, _coolingLastError;
    bool _coolingFirstRun;

    // Plant model — per-zone heating/cooling rates (°C/s) and deadtime (s)
    float _heatingRate[NUM_ZONES];
    float _coolingRate[NUM_ZONES];
    float _deadtime[NUM_ZONES];

    // Deadtime step-detection state
    float _prevOutput;
    float _prevDTempDt;
    bool _waitingForTempRise;
    int _deadtimeRiseCycle;
    int _totalSamples;

    // Line frequency calibration
    volatile bool _calibrating;
    volatile int _calCount;
    volatile uint64_t _calStartTime;
    volatile uint64_t _calEndTime;
    volatile bool _calDone;
    float _halfPeriodUs;
    float _freqHz;

    void calibrateLineFrequency();
    float computeFeedforward(float dTargetDt, ProfileStage stage) const;
    float computeCoolingFeedforward(float dTargetDt, ProfileStage stage) const;
};

#endif
