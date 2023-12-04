#include <stdint.h>

#define RX5808_MIN_TUNETIME 35    // after set freq need to wait this long before read RSSI
#define RX5808_MIN_BUSTIME 30     // after set freq need to wait this long before setting again
#define POWER_DOWN_FREQ_MHZ 1111  // signal to power down the module
#define RSSI_READS 5              // number of analog RSSI reads per tick

class RX5808 {
   public:
    RX5808(uint8_t _rssiInputPin, uint8_t _rx5808DataPin, uint8_t _rx5808SelPin, uint8_t _rx5808ClkPin);
    void init();
    void setFrequency(uint16_t frequency);
    uint8_t readRssi();
    void handleFrequencyChange(uint32_t currentTimeMs, uint16_t potentiallyNewFreq);

   private:
    uint8_t rx5808DataPin = 0;  // DATA (CH1) output line to RX5808 module
    uint8_t rx5808ClkPin = 0;   // CLK (CH3) output line to RX5808 module
    uint8_t rx5808SelPin = 0;   // SEL (CH2) output line to RX5808 module
    uint8_t rssiInputPin = 0;   // RSSI input from RX5808

    uint16_t currentFrequency = 0;

    bool rxPoweredDown = false;
    bool recentSetFreqFlag = false;
    uint32_t lastSetFreqTimeMs = 0;

    void rx5808SerialSendBit1();
    void rx5808SerialSendBit0();
    void rx5808SerialEnableLow();
    void rx5808SerialEnableHigh();

    void setRxModulePower(uint32_t options);
    void resetRxModule();
    void setupRxModule();
    void powerDownRxModule();
    bool verifyFrequency();

    static uint16_t freqMhzToRegVal(uint16_t freqInMhz);
};
