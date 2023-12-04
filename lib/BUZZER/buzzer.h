#include <Arduino.h>

#pragma once

typedef enum {
    BUZZER_IDLE,
    BUZZER_BEEPING
} buzzer_state_e;

class Buzzer {
   public:
    void init(uint8_t pin, bool inverted);
    void handleBuzzer(uint32_t currentTimeMs);
    void beep(uint32_t timeMs);

   private:
    buzzer_state_e buzzerState = BUZZER_IDLE;
    uint8_t buzzerPin;
    uint8_t initialState = LOW;
    uint32_t beepTimeMs;
    uint32_t startTimeMs;
};
