#include "Buzzer.h"

Buzzer::Buzzer()
    : _activePattern(BUZZER_OFF), _patternStart(0),
      _state(false), _beepCount(0), _currentBeep(0) {}

void Buzzer::begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

void Buzzer::trigger(BuzzerPattern pattern) {
    _activePattern = pattern;
    _patternStart = millis();
    _currentBeep = 0;
    _state = false;
    _beepCount = 0;

    switch (pattern) {
        case BUZZER_PHASE_TRANSITION:
            _beepCount = 1;
            break;
        case BUZZER_CYCLE_COMPLETE:
            _beepCount = 3;
            break;
        case BUZZER_ERROR_SENSOR:
            _beepCount = 10;
            break;
        case BUZZER_ERROR_OVERTEMP:
        case BUZZER_ESTOP:
            _beepCount = -1;
            break;
        default:
            break;
    }
}

void Buzzer::update() {
    if (_activePattern == BUZZER_OFF) {
        digitalWrite(PIN_BUZZER, LOW);
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - _patternStart;

    if (_activePattern == BUZZER_ERROR_OVERTEMP || _activePattern == BUZZER_ESTOP) {
        bool onState = (elapsed % 500) < 250;
        digitalWrite(PIN_BUZZER, onState ? HIGH : LOW);
        return;
    }

    if (_activePattern == BUZZER_ERROR_SENSOR) {
        bool onState = (elapsed % 200) < 100;
        digitalWrite(PIN_BUZZER, onState ? HIGH : LOW);
        if (elapsed > 5000) stop();
        return;
    }

    if (_beepCount > 0 && _currentBeep < _beepCount) {
        int beepDuration = (_activePattern == BUZZER_CYCLE_COMPLETE) ? 500 : 150;
        int pauseDuration = (_activePattern == BUZZER_CYCLE_COMPLETE) ? 300 : 150;
        int totalBeepTime = (beepDuration + pauseDuration);

        int beepPhase = elapsed % totalBeepTime;
        int currentBeepIdx = elapsed / totalBeepTime;

        if (currentBeepIdx >= _beepCount) {
            stop();
            return;
        }

        _currentBeep = currentBeepIdx;
        digitalWrite(PIN_BUZZER, (beepPhase < beepDuration) ? HIGH : LOW);
    }
}

void Buzzer::stop() {
    _activePattern = BUZZER_OFF;
    digitalWrite(PIN_BUZZER, LOW);
}
