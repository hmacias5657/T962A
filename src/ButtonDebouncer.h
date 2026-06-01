#ifndef BUTTON_DEBOUNCER_H
#define BUTTON_DEBOUNCER_H

#include <Arduino.h>
#include "Config.h"

struct ButtonState {
    int pin;
    bool lastSteadyState;
    bool lastFlickerState;
    unsigned long lastDebounceTime;
    bool activeLow;

    ButtonState(int p, bool active = true)
        : pin(p), lastSteadyState(HIGH), lastFlickerState(HIGH),
          lastDebounceTime(0), activeLow(active) {}
};

class ButtonDebouncer {
public:
    ButtonDebouncer();

    void begin();
    bool isPressed(ButtonState &btn);

    // T962A original membrane keys
    ButtonState btnF1;        // F1 = UP
    ButtonState btnF2;        // F2 = DOWN
    ButtonState btnF3;        // F3 = LEFT
    ButtonState btnF4;        // F4 = RIGHT
    ButtonState btnS;         // S  = SELECT/START

    // Dedicated hardware emergency stop
    ButtonState btnEStop;

    // Convenience aliases for code readability
    ButtonState &btnUp    = btnF1;
    ButtonState &btnDown  = btnF2;
    ButtonState &btnLeft  = btnF3;
    ButtonState &btnRight = btnF4;
    ButtonState &btnSelect = btnS;
    ButtonState &btnStart  = btnS;
};

#endif
