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
    void runPIDLoop(int targetTemp, float avgTemp, float temp1, float temp2);

    // Cooling channel
    float getCoolingKp() const { return _coolingKp; }
    float getCoolingKi() const { return _coolingKi; }
    float getCoolingKd() const { return _coolingKd; }
    void setCoolingGains(float kp, float ki, float kd);
    void setCoolingOutput(int output) { _coolingOutput = constrain(output, 0, BRESENHAM_CYCLES); }
    int getCoolingOutput() const { return _coolingOutput; }
    void runCoolingPID(int targetTemp, float avgTemp);

    // Common
    void reset();
    bool isAdcTriggered() const { return _triggerAdcRead; }
    void clearAdcTrigger() { _triggerAdcRead = false; }
    void emergencyStop();

    // Line frequency calibration
    static const int CAL_CYCLES = 40;
    float getMeasuredFrequency() const { return _freqHz; }
    float getMeasuredHalfPeriodUs() const { return _halfPeriodUs; }
    float getWindowMs() const;
    float getTimeStepS() const;
    void setMeasuredFrequency(float freqHz);

private:
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

    // Line frequency calibration
    volatile bool _calibrating;
    volatile int _calCount;
    volatile uint64_t _calStartTime;
    volatile uint64_t _calEndTime;
    volatile bool _calDone;
    float _halfPeriodUs;
    float _freqHz;

    void calibrateLineFrequency();
};

#endif
