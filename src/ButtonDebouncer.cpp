#include "ButtonDebouncer.h"

ButtonDebouncer::ButtonDebouncer()
    : btnF1(PIN_BTN_F1_UP), btnF2(PIN_BTN_F2_DOWN),
      btnF3(PIN_BTN_F3_LEFT), btnF4(PIN_BTN_F4_RIGHT),
      btnS(PIN_BTN_S),
      btnEStop(PIN_BTN_ESTOP, false) {}

void ButtonDebouncer::begin() {
    pinMode(PIN_BTN_F1_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_F2_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_F3_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_F4_RIGHT, INPUT_PULLUP);
    pinMode(PIN_BTN_S, INPUT_PULLUP);
    pinMode(PIN_BTN_ESTOP, INPUT_PULLUP);

    btnF1.lastSteadyState = digitalRead(PIN_BTN_F1_UP);
    btnF1.lastFlickerState = btnF1.lastSteadyState;

    btnF2.lastSteadyState = digitalRead(PIN_BTN_F2_DOWN);
    btnF2.lastFlickerState = btnF2.lastSteadyState;

    btnF3.lastSteadyState = digitalRead(PIN_BTN_F3_LEFT);
    btnF3.lastFlickerState = btnF3.lastSteadyState;

    btnF4.lastSteadyState = digitalRead(PIN_BTN_F4_RIGHT);
    btnF4.lastFlickerState = btnF4.lastSteadyState;

    btnS.lastSteadyState = digitalRead(PIN_BTN_S);
    btnS.lastFlickerState = btnS.lastSteadyState;

    btnEStop.lastSteadyState = digitalRead(PIN_BTN_ESTOP);
    btnEStop.lastFlickerState = btnEStop.lastSteadyState;
}

bool ButtonDebouncer::isPressed(ButtonState &btn) {
    bool currentReading = digitalRead(btn.pin);
    bool pressedTriggered = false;

    if (currentReading != btn.lastFlickerState) {
        btn.lastDebounceTime = millis();
        btn.lastFlickerState = currentReading;
    }

    if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (currentReading != btn.lastSteadyState) {
            btn.lastSteadyState = currentReading;

            if (btn.activeLow) {
                if (btn.lastSteadyState == LOW) pressedTriggered = true;
            } else {
                if (btn.lastSteadyState == HIGH) pressedTriggered = true;
            }
        }
    }

    return pressedTriggered;
}
