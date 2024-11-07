#include "buzzer.h"

void Buzzer::init(uint8_t pin, bool inverted) {
    pinMode(pin, OUTPUT);
    initialState = inverted ? HIGH : LOW;
    buzzerPin = pin;
    buzzerState = BUZZER_IDLE;
    digitalWrite(buzzerPin, initialState);
}

void Buzzer::beep(uint32_t timeMs) {
    beepTimeMs = timeMs;
    buzzerState = BUZZER_BEEPING;
    startTimeMs = millis();
    digitalWrite(buzzerPin, !initialState);
}

void Buzzer::handleBuzzer(uint32_t currentTimeMs) {
    switch (buzzerState) {
        case BUZZER_IDLE:
            break;
        case BUZZER_BEEPING:
            if (currentTimeMs < startTimeMs) {
                break;  // updated from different core
            }
            if ((currentTimeMs - startTimeMs) > beepTimeMs) {
                digitalWrite(buzzerPin, initialState);
                buzzerState = BUZZER_IDLE;
            }
            break;
        default:
            break;
    }
}
