#include "BresenhamPID.h"

BresenhamPID::BresenhamPID()
    : _bresenhamOutput(0), _coolingOutput(0), _triggerAdcRead(false), _zcCycleCount(0),
      _Kp(2.0f), _Ki(0.05f), _Kd(1.2f),
      _integral(0.0f), _lastError(0.0f), _firstRun(true),
      _coolingKp(1.0f), _coolingKi(0.01f), _coolingKd(0.5f),
      _coolingIntegral(0.0f), _coolingLastError(0.0f), _coolingFirstRun(true),
      _calibrating(false), _calCount(0), _calStartTime(0), _calEndTime(0), _calDone(false),
      _halfPeriodUs(8333.0f), _freqHz(DEFAULT_FREQ_HZ) {}

static BresenhamPID* _pidInstance = nullptr;

void IRAM_ATTR _isrZeroCrossing() {
    if (_pidInstance) _pidInstance->handleZeroCrossing();
}

void BresenhamPID::begin() {
    _pidInstance = this;
    pinMode(PIN_SSR_GATE, OUTPUT);
    digitalWrite(PIN_SSR_GATE, LOW);
    pinMode(PIN_COOLING_FAN_SSR, OUTPUT);
    digitalWrite(PIN_COOLING_FAN_SSR, LOW);
    pinMode(PIN_ZC_INTERRUPT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ZC_INTERRUPT), _isrZeroCrossing, RISING);

    calibrateLineFrequency();
}

void IRAM_ATTR BresenhamPID::handleZeroCrossing() {
    // Calibration sampling
    if (_calibrating) {
        if (_calCount == 0) {
            _calStartTime = esp_timer_get_time();
            _calCount = 1;
        } else {
            _calCount++;
            if (_calCount >= CAL_CYCLES) {
                _calEndTime = esp_timer_get_time();
                _calibrating = false;
                _calDone = true;
            }
        }
    }

    // Heater Bresenham accumulator
    static int heaterAccum = 0;
    heaterAccum += _bresenhamOutput;
    if (heaterAccum >= BRESENHAM_CYCLES) {
        digitalWrite(PIN_SSR_GATE, HIGH);
        heaterAccum -= BRESENHAM_CYCLES;
    } else {
        digitalWrite(PIN_SSR_GATE, LOW);
    }

    // Cooling fan Bresenham accumulator
    static int coolingAccum = 0;
    coolingAccum += _coolingOutput;
    if (coolingAccum >= BRESENHAM_CYCLES) {
        digitalWrite(PIN_COOLING_FAN_SSR, HIGH);
        coolingAccum -= BRESENHAM_CYCLES;
    } else {
        digitalWrite(PIN_COOLING_FAN_SSR, LOW);
    }

    // ADC trigger every BRESENHAM_CYCLES half-cycles
    _zcCycleCount++;
    if (_zcCycleCount >= BRESENHAM_CYCLES) {
        _zcCycleCount = 0;
        _triggerAdcRead = true;
    }
}

void BresenhamPID::setAIGains(float kp, float ki, float kd) {
    _Kp = constrain(kp, 0.0f, 50.0f);
    _Ki = constrain(ki, 0.0f, 10.0f);
    _Kd = constrain(kd, 0.0f, 50.0f);
}

void BresenhamPID::setCoolingGains(float kp, float ki, float kd) {
    _coolingKp = constrain(kp, 0.0f, 50.0f);
    _coolingKi = constrain(ki, 0.0f, 10.0f);
    _coolingKd = constrain(kd, 0.0f, 50.0f);
}

void BresenhamPID::reset() {
    _integral = 0.0f;
    _lastError = 0.0f;
    _firstRun = true;
    _bresenhamOutput = 0;
    _coolingIntegral = 0.0f;
    _coolingLastError = 0.0f;
    _coolingFirstRun = true;
    _coolingOutput = 0;
    _triggerAdcRead = false;
    _zcCycleCount = 0;
    // Keep calibration values - frequency doesn't change on reset
}

void BresenhamPID::runPIDLoop(int targetTemp, float avgTemp, float temp1, float temp2) {
    float error = (float)targetTemp - avgTemp;
    float ts = getTimeStepS();

    if (_firstRun) {
        _lastError = error;
        _firstRun = false;
    }

    _integral += error * ts;
    _integral = constrain(_integral, -500.0f, 500.0f);

    float derivative = (error - _lastError) / ts;
    _lastError = error;

    float controlValue = (_Kp * error) + (_Ki * _integral) + (_Kd * derivative);
    _bresenhamOutput = constrain((int)controlValue, 0, BRESENHAM_CYCLES);
}

void BresenhamPID::runCoolingPID(int targetTemp, float avgTemp) {
    float error = avgTemp - (float)targetTemp;
    float ts = getTimeStepS();

    if (_coolingFirstRun) {
        _coolingLastError = error;
        _coolingFirstRun = false;
    }

    _coolingIntegral += error * ts;
    _coolingIntegral = constrain(_coolingIntegral, -500.0f, 500.0f);

    float derivative = (error - _coolingLastError) / ts;
    _coolingLastError = error;

    float controlValue = (_coolingKp * error) + (_coolingKi * _coolingIntegral) + (_coolingKd * derivative);
    _coolingOutput = constrain((int)controlValue, 0, BRESENHAM_CYCLES);
}

void BresenhamPID::emergencyStop() {
    _bresenhamOutput = 0;
    _coolingOutput = 0;
    digitalWrite(PIN_SSR_GATE, LOW);
    digitalWrite(PIN_COOLING_FAN_SSR, LOW);
}

void BresenhamPID::calibrateLineFrequency() {
    _calibrating = true;
    _calCount = 0;
    _calDone = false;

    unsigned long timeout = millis() + 2000;
    while (!_calDone && millis() < timeout) {
        delay(1);
    }

    if (!_calDone) {
        _halfPeriodUs = 8333.0f;
        _freqHz = DEFAULT_FREQ_HZ;
        return;
    }

    uint64_t totalUs = _calEndTime - _calStartTime;
    _halfPeriodUs = (float)totalUs / (float)(CAL_CYCLES - 1);

    _freqHz = 1000000.0f / (2.0f * _halfPeriodUs);

    if (_freqHz < 45.0f || _freqHz > 66.0f) {
        _halfPeriodUs = 8333.0f;
        _freqHz = DEFAULT_FREQ_HZ;
    }
}

float BresenhamPID::getWindowMs() const {
    return (BRESENHAM_CYCLES * _halfPeriodUs) / 1000.0f;
}

float BresenhamPID::getTimeStepS() const {
    return getWindowMs() / 1000.0f;
}

void BresenhamPID::setMeasuredFrequency(float freqHz) {
    _freqHz = constrain(freqHz, 45.0f, 66.0f);
    _halfPeriodUs = 1000000.0f / (2.0f * _freqHz);
}
