#ifndef SUBGHZ_INTEGRATION_H
#define SUBGHZ_INTEGRATION_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <PCF8574.h>
#include <ELECHOUSE_CC1101.h>
#include <RCSwitch.h>
#include <arduinoFFT.h>
#include <EEPROM.h>

// External references
extern TFT_eSPI tft;
extern PCF8574 pcf;

// Pin definitions for subGHz operations
#define SUBGHZ_RX_PIN 16
#define SUBGHZ_TX_PIN 27
#define CC1101_SCK_PIN 18
#define CC1101_MISO_PIN 19
#define CC1101_MOSI_PIN 23
#define CC1101_CS_PIN 26

// Pin sharing control
extern bool nrf24_active;
extern bool cc1101_active;
extern bool feature_exit_requested;
extern bool feature_active;

// Function declarations
void activateCC1101();
void activateNRF24();
void setupTouchscreen();
float readBatteryVoltage();
void drawStatusBar(float voltage, bool charging);

// Touchscreen removed

// SubGHz Replay Attack with Spectrum Analysis
namespace SubGHzReplayAttack {
    void setup();
    void loop();
    void updateDisplay();
    void sendSignal();
    void saveProfile();
    void loadProfile();
    void runUI();
}

// SubGHz Scanner with Spectrum Analysis
namespace SubGHzScanner {
    void setup();
    void loop();
    void updateDisplay();
    void runUI();
    void do_sampling();
}

// SubGHz Jammer with Spectrum Analysis
namespace SubGHzJammer {
    void setup();
    void loop();
    void updateDisplay();
    void runUI();
}

#endif // SUBGHZ_INTEGRATION_H
