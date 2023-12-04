#include "led.h"

void Led::init(uint8_t pin, bool inverted) {
    pinMode(pin, OUTPUT);
    initialState = inverted ? HIGH : LOW;
    currentState = initialState;
    ledPin = pin;
    ledState = LED_IDLE;
    digitalWrite(ledPin, initialState);
}

void Led::on(uint32_t timeMs) {
    if (timeMs > 0) {
        ledState = LED_ON;
        onTimeMs = timeMs;
        checkTimeMs = millis();
    } else {
        ledState = LED_IDLE;
    }
    digitalWrite(ledPin, !initialState);
}

void Led::off() {
    ledState = LED_IDLE;
    digitalWrite(ledPin, initialState);
}

void Led::blink(uint32_t onMs, uint32_t offMs) {
    onTimeMs = onMs;
    if (offMs > 0) {
        offTimeMs = offMs;
    } else {
        offTimeMs = onTimeMs;
    }
    ledState = LED_BLINKING;
    checkTimeMs = millis();
    currentState = !initialState;
    digitalWrite(ledPin, currentState);
}

void Led::handleLed(uint32_t currentTimeMs) {
    switch (ledState) {
        case LED_IDLE:
            break;
        case LED_BLINKING:
            if (currentTimeMs < checkTimeMs) {
                break;  // updated from different core
            }
            if (((currentState == !initialState) && (currentTimeMs - checkTimeMs) > onTimeMs) ||  // currently led is turned on and it's time to turn it off
                ((currentState == initialState) && (currentTimeMs - checkTimeMs) > offTimeMs)) {  // currently led is turned off and it's time to turn it on
                currentState = !currentState;
                digitalWrite(ledPin, currentState);
                checkTimeMs = currentTimeMs;
            }
            break;
        case LED_ON:
            if (currentTimeMs < checkTimeMs) {
                break;  // updated from different core
            }
            if ((currentTimeMs - checkTimeMs) > onTimeMs) {
                digitalWrite(ledPin, initialState);
                ledState = LED_IDLE;
            }
        default:
            break;
    }
}
