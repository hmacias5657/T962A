#ifndef TEMPERATURE_READER_H
#define TEMPERATURE_READER_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "Config.h"

struct TemperatureData {
    float temp1;
    float temp2;
    float avgTemp;
    float spatialDelta;
    bool sensor1Fault;
    bool sensor2Fault;
};

class TemperatureReader {
public:
    TemperatureReader();

    void begin();
    TemperatureData readSensors();
    void setCalibrationOffset(int sensor, float offset);
    float getCalibrationOffset(int sensor) const;
    void resetFilter() { _firstRead = true; }

private:
    Adafruit_ADS1015 _ads;
    float _tc1Offset;
    float _tc2Offset;
    float _filteredTemp1 = 0.0f;
    float _filteredTemp2 = 0.0f;
    bool _firstRead = true;
    static constexpr float _alpha = 0.15f;
    float rawToCelsius(int16_t raw);
};

#endif
