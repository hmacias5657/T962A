#include "TemperatureReader.h"

TemperatureReader::TemperatureReader() : _tc1Offset(0.0f), _tc2Offset(0.0f) {
    resetFilter();
}

void TemperatureReader::begin() {
    if (!_ads.begin(ADS1015_ADDR)) {
        Serial.println("FATAL: ADS1015 not found");
        while (1) { delay(100); }
    }
    _ads.setGain(GAIN_ONE);
    Serial.println("ADS1015 initialized");
}

float TemperatureReader::rawToCelsius(int16_t raw) {
    float voltage = _ads.computeVolts(raw);
    return voltage / TC_AMP_SENSITIVITY;
}

void TemperatureReader::setCalibrationOffset(int sensor, float offset) {
    if (sensor == 0) _tc1Offset = constrain(offset, CAL_OFFSET_MIN, CAL_OFFSET_MAX);
    else _tc2Offset = constrain(offset, CAL_OFFSET_MIN, CAL_OFFSET_MAX);
}

float TemperatureReader::getCalibrationOffset(int sensor) const {
    return (sensor == 0) ? _tc1Offset : _tc2Offset;
}

TemperatureData TemperatureReader::readSensors() {
    TemperatureData data;

    int16_t rawTC1 = _ads.readADC_SingleEnded(0);
    int16_t rawTC2 = _ads.readADC_SingleEnded(1);

    float raw1 = rawToCelsius(rawTC1) + _tc1Offset;
    float raw2 = rawToCelsius(rawTC2) + _tc2Offset;

    if (_firstRead) {
        _filteredTemp1 = raw1;
        _filteredTemp2 = raw2;
        _firstRead = false;
    } else {
        _filteredTemp1 = _alpha * raw1 + (1.0f - _alpha) * _filteredTemp1;
        _filteredTemp2 = _alpha * raw2 + (1.0f - _alpha) * _filteredTemp2;
    }

    data.temp1 = _filteredTemp1;
    data.temp2 = _filteredTemp2;
    data.avgTemp = (data.temp1 + data.temp2) / 2.0f;
    data.spatialDelta = fabs(data.temp1 - data.temp2);
    data.sensor1Fault = (raw1 < SENSOR_FAULT_TEMP);
    data.sensor2Fault = (raw2 < SENSOR_FAULT_TEMP);

    return data;
}
