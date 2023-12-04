#include <stdint.h>

#include "buzzer.h"
#include "led.h"

#define MONITOR_CHECK_TIME_MS 5000
#define MONITOR_BEEP_TIME_MS 500
#define AVERAGING_SIZE 5

typedef enum {
    ALARM_OFF,
    ALARM_IDLE,
    ALARM_BEEPING
} alarm_state_e;

class BatteryMonitor {
   public:
    void init(uint8_t pin, uint8_t batScale, uint8_t batAdd, Buzzer *buzzer, Led *l);
    uint8_t getBatteryVoltage();
    void checkBatteryState(uint32_t currentTimeMs, uint8_t alarmThreshold);

   private:
    alarm_state_e state = ALARM_OFF;
    uint16_t measurements[AVERAGING_SIZE];
    uint8_t measurementIndex = 0;
    uint32_t lastCheckTimeMs = 0;
    uint8_t vbatPin = 0;
    uint8_t scale = 1;
    uint8_t add = 0;
    Buzzer *buz;
    Led *led;
};
