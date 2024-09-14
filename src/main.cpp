#include "debug.h"
#include "led.h"
#include "webserver.h"

#ifdef ESP32_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

static RX5808 rx(PIN_RX5808_RSSI, PIN_RX5808_DATA, PIN_RX5808_SELECT, PIN_RX5808_CLOCK);
static Config config;
static Webserver ws;
static Buzzer buzzer;
static Led led;
static LapTimer timer;
static BatteryMonitor monitor;

#ifdef ESP32_OLED
static Adafruit_SSD1306 display(128, 64, &Wire, -1);
#endif

static TaskHandle_t xTimerTask = NULL;

#ifdef ESP32_OLED
static TaskHandle_t xDisplayTask = NULL;
#endif

static void parallelTask(void *pvArgs) {
    for (;;) {
        uint32_t currentTimeMs = millis();
        ws.handleWebUpdate(currentTimeMs);
        config.handleEeprom(currentTimeMs);
        rx.handleFrequencyChange(currentTimeMs, config.getFrequency());
        monitor.checkBatteryState(currentTimeMs, config.getAlarmThreshold());
        buzzer.handleBuzzer(currentTimeMs);
        led.handleLed(currentTimeMs);
    }
}

static void initParallelTask() {
    disableCore0WDT();
    xTaskCreatePinnedToCore(parallelTask, "parallelTask", 3000, NULL, 0, &xTimerTask, 0);
}

#ifdef ESP32_OLED
static void updateDisplayTask(void *pvArgs) {
    for (;;) {
        // Update display
        display.clearDisplay();
        display.setCursor(0,0);
        if (WiFi.getMode() == WIFI_AP) {
            display.println(WiFi.softAPSSID());
        } else {
            display.println("PhobosLT");
        }
        display.print("Batt: ");
        display.print(monitor.getBatteryVoltage() / 10.0);
        display.println("V");
        display.println(config.getFrequencyName());
        display.print(config.getFrequency() / 1000.0, 3);
        display.println(" GHz");
        display.print("Pilot: ");
        display.println(config.getPilotName());
        display.print("Last Lap: ");
        display.print(timer.getLastLapTime() / 1000.0);
        display.println("s");
        display.display();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update display every 100ms
    }
}

static void initDisplayTask() {
    xTaskCreatePinnedToCore(updateDisplayTask, "updateDisplayTask", 3000, NULL, 1, &xDisplayTask, 1);
}
#endif

void setup() {
    DEBUG_INIT;
    config.init();
    rx.init();
    buzzer.init(PIN_BUZZER, BUZZER_INVERTED);
    led.init(PIN_LED, true); //was false
    timer.init(&config, &rx, &buzzer, &led);
    monitor.init(PIN_VBAT, VBAT_SCALE, VBAT_ADD, &buzzer, &led);
    ws.init(&config, &timer, &monitor, &buzzer, &led);
    led.on(400);
    buzzer.beep(200);
    initParallelTask();
    
    #ifdef ESP32_OLED
    // Initialize OLED display
    Wire.begin(5, 4);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        DEBUG("SSD1306 allocation failed\n");
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("PhobosLT");
    display.display();

    // Start the display update task
    initDisplayTask();
    #endif
}

void loop() {
    uint32_t currentTimeMs = millis();
    timer.handleLapTimerUpdate(currentTimeMs);
}
