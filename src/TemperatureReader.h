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

private:
    Adafruit_ADS1015 _ads;
    float _tc1Offset;
    float _tc2Offset;
    float rawToCelsius(int16_t raw);
};

#endif
