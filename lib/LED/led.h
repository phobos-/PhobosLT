#include <Arduino.h>

#pragma once

typedef enum {
    LED_IDLE,
    LED_BLINKING,
    LED_ON
} led_state_e;

class Led {
   public:
    void init(uint8_t pin, bool inverted);
    void handleLed(uint32_t currentTimeMs);
    void on(uint32_t timeMs = 0);
    void off();
    void blink(uint32_t onTimeMs, uint32_t offTimeMs = 0);

   private:
    led_state_e ledState = LED_IDLE;
    uint8_t ledPin;
    uint8_t initialState = LOW;
    uint8_t currentState = LOW;
    uint32_t onTimeMs;
    uint32_t offTimeMs;
    uint32_t checkTimeMs;
};
