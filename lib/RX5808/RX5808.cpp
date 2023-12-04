#include "RX5808.h"

#include <Arduino.h>

#include "debug.h"

RX5808::RX5808(uint8_t _rssiInputPin, uint8_t _rx5808DataPin, uint8_t _rx5808SelPin, uint8_t _rx5808ClkPin) {
    rssiInputPin = _rssiInputPin;
    rx5808DataPin = _rx5808DataPin;
    rx5808SelPin = _rx5808SelPin;
    rx5808ClkPin = _rx5808ClkPin;
    lastSetFreqTimeMs = millis();
}

void RX5808::init() {
    pinMode(rssiInputPin, INPUT);
    pinMode(rx5808DataPin, OUTPUT);
    pinMode(rx5808SelPin, OUTPUT);
    pinMode(rx5808ClkPin, OUTPUT);
    digitalWrite(rx5808SelPin, HIGH);
    digitalWrite(rx5808ClkPin, LOW);
    digitalWrite(rx5808DataPin, LOW);
    resetRxModule();
    setFrequency(POWER_DOWN_FREQ_MHZ);
}

void RX5808::handleFrequencyChange(uint32_t currentTimeMs, uint16_t potentiallyNewFreq) {
    if ((currentFrequency != potentiallyNewFreq) && ((currentTimeMs - lastSetFreqTimeMs) > RX5808_MIN_BUSTIME)) {
        lastSetFreqTimeMs = currentTimeMs;
        setFrequency(potentiallyNewFreq);
    }

    if (recentSetFreqFlag && (currentTimeMs - lastSetFreqTimeMs) > RX5808_MIN_TUNETIME) {
        lastSetFreqTimeMs = currentTimeMs;
        DEBUG("RX5808 Tune done\n");
        verifyFrequency();
        recentSetFreqFlag = false;  // don't need to check again until next freq change
    }
}

bool RX5808::verifyFrequency() {
    // Start of Read Reg code :
    // Verify read HEX value in RX5808 module Frequency Register 0x01
    uint16_t vtxRegisterHex = 0;
    //  Modified copy of packet code in setRxModuleToFreq(), to read Register 0x01
    //  20 bytes of register data are read, but the
    //  MSB 4 bits are zeros
    //  Data Packet is: register address (4-bits) = 0x1, read/write bit = 1 for read, data D0-D15 stored in vtxHexVerify, data15-19=0x0

    rx5808SerialEnableHigh();
    rx5808SerialEnableLow();

    rx5808SerialSendBit1();  // Register 0x1
    rx5808SerialSendBit0();
    rx5808SerialSendBit0();
    rx5808SerialSendBit0();

    rx5808SerialSendBit0();  // Read register r/w

    // receive data D0-D15, and ignore D16-D19
    pinMode(rx5808DataPin, INPUT_PULLUP);
    for (uint8_t i = 0; i < 20; i++) {
        delayMicroseconds(10);
        // only use D0-D15, ignore D16-D19
        if (i < 16) {
            if (digitalRead(rx5808DataPin)) {
                bitWrite(vtxRegisterHex, i, 1);
            } else {
                bitWrite(vtxRegisterHex, i, 0);
            }
        }
        if (i >= 16) {
            digitalRead(rx5808DataPin);
        }
        digitalWrite(rx5808ClkPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(rx5808ClkPin, LOW);
        delayMicroseconds(10);
    }

    pinMode(rx5808DataPin, OUTPUT);  // return status of Data pin after INPUT_PULLUP above
    rx5808SerialEnableHigh();        // Finished clocking data in
    delay(2);

    digitalWrite(rx5808ClkPin, LOW);
    digitalWrite(rx5808DataPin, LOW);

    if (vtxRegisterHex != freqMhzToRegVal(currentFrequency)) {
        DEBUG("RX5808 frequency not matching, register = %u, currentFreq = %u\n", vtxRegisterHex, currentFrequency);
        return false;
    }
    DEBUG("RX5808 frequency verified properly\n");
    return true;
}

// Set frequency on RX5808 module to given value
void RX5808::setFrequency(uint16_t vtxFreq) {
    DEBUG("Setting frequency to %u\n", vtxFreq);

    currentFrequency = vtxFreq;

    if (vtxFreq == POWER_DOWN_FREQ_MHZ)  // frequency value to power down rx module
    {
        powerDownRxModule();
        rxPoweredDown = true;
        return;
    }
    if (rxPoweredDown) {
        resetRxModule();
        rxPoweredDown = false;
    }

    // Get the hex value to send to the rx module
    uint16_t vtxHex = freqMhzToRegVal(vtxFreq);

    // Channel data from the lookup table, 20 bytes of register data are sent, but the
    // MSB 4 bits are zeros register address = 0x1, write, data0-15=vtxHex data15-19=0x0
    rx5808SerialEnableHigh();
    rx5808SerialEnableLow();

    rx5808SerialSendBit1();  // Register 0x1
    rx5808SerialSendBit0();
    rx5808SerialSendBit0();
    rx5808SerialSendBit0();

    rx5808SerialSendBit1();  // Write to register

    // D0-D15, note: loop runs backwards as more efficent on AVR
    uint8_t i;
    for (i = 16; i > 0; i--) {
        if (vtxHex & 0x1) {  // Is bit high or low?
            rx5808SerialSendBit1();
        } else {
            rx5808SerialSendBit0();
        }
        vtxHex >>= 1;  // Shift bits along to check the next one
    }

    for (i = 4; i > 0; i--)  // Remaining D16-D19
        rx5808SerialSendBit0();

    rx5808SerialEnableHigh();  // Finished clocking data in
    delay(2);

    digitalWrite(rx5808ClkPin, LOW);
    digitalWrite(rx5808DataPin, LOW);

    recentSetFreqFlag = true;  // indicate need to wait RX5808_MIN_TUNETIME before reading RSSI
}

// Read the RSSI value
uint8_t RX5808::readRssi() {
    volatile uint16_t rssi = 0;

    if (recentSetFreqFlag) return rssi;  // RSSI is unstable

    // for (uint8_t i = 0; i < RSSI_READS; i++) {
    //   rssi += map(analogRead(rssiInputPin), 0, analogRead(vbatPin), 0, 4095);
    // }

    // rssi = rssi / RSSI_READS; // average of RSSI_READS readings

    // reads 5V value as 0-4095, RX5808 is 3.3V powered so RSSI pin will never output the full range
    rssi = analogRead(rssiInputPin);
    // clamp upper range to fit scaling
    if (rssi > 2047) rssi = 2047;
    // rescale to fit into a byte and remove some jitter TODO: experiment with exp or log
    return rssi >> 3;
}

void RX5808::rx5808SerialSendBit1() {
    digitalWrite(rx5808DataPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(rx5808ClkPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(rx5808ClkPin, LOW);
    delayMicroseconds(300);
}

void RX5808::rx5808SerialSendBit0() {
    digitalWrite(rx5808DataPin, LOW);
    delayMicroseconds(300);
    digitalWrite(rx5808ClkPin, HIGH);
    delayMicroseconds(300);
    digitalWrite(rx5808ClkPin, LOW);
    delayMicroseconds(300);
}

void RX5808::rx5808SerialEnableLow() {
    digitalWrite(rx5808SelPin, LOW);
    delayMicroseconds(200);
}

void RX5808::rx5808SerialEnableHigh() {
    digitalWrite(rx5808SelPin, HIGH);
    delayMicroseconds(200);
}

// Reset rx5808 module to wake up from power down
void RX5808::resetRxModule() {
    rx5808SerialEnableHigh();
    rx5808SerialEnableLow();

    rx5808SerialSendBit1();  // Register 0xF
    rx5808SerialSendBit1();
    rx5808SerialSendBit1();
    rx5808SerialSendBit1();

    rx5808SerialSendBit1();  // Write to register

    for (uint8_t i = 20; i > 0; i--)
        rx5808SerialSendBit0();

    rx5808SerialEnableHigh();  // Finished clocking data in

    setupRxModule();
}

// Set power options on the rx5808 module
void RX5808::setRxModulePower(uint32_t options) {
    rx5808SerialEnableHigh();
    rx5808SerialEnableLow();

    rx5808SerialSendBit0();  // Register 0xA
    rx5808SerialSendBit1();
    rx5808SerialSendBit0();
    rx5808SerialSendBit1();

    rx5808SerialSendBit1();  // Write to register

    for (uint8_t i = 20; i > 0; i--) {
        if (options & 0x1) {  // Is bit high or low?
            rx5808SerialSendBit1();
        } else {
            rx5808SerialSendBit0();
        }
        options >>= 1;  // Shift bits along to check the next one
    }

    rx5808SerialEnableHigh();  // Finished clocking data in

    digitalWrite(rx5808DataPin, LOW);
}

// Power down rx5808 module
void RX5808::powerDownRxModule() {
    setRxModulePower(0b11111111111111111111);
}

// Set up rx5808 module (disabling unused features to save some power)
void RX5808::setupRxModule() {
    setRxModulePower(0b11010000110111110011);
}

// Calculate rx5808 register hex value for given frequency in MHz
uint16_t RX5808::freqMhzToRegVal(uint16_t freqInMhz) {
    uint16_t tf, N, A;
    tf = (freqInMhz - 479) / 2;
    N = tf / 32;
    A = tf % 32;
    return (N << (uint16_t)7) + A;
}
