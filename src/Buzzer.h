#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "Config.h"

enum BuzzerPattern : uint8_t {
    BUZZER_OFF = 0,
    BUZZER_PHASE_TRANSITION,
    BUZZER_CYCLE_COMPLETE,
    BUZZER_ERROR_SENSOR,
    BUZZER_ERROR_OVERTEMP,
    BUZZER_ESTOP
};

class Buzzer {
public:
    Buzzer();

    void begin();
    void trigger(BuzzerPattern pattern);
    void update();
    void stop();

private:
    BuzzerPattern _activePattern;
    unsigned long _patternStart;
    bool _state;
    int _beepCount;
    int _currentBeep;
};

#endif
