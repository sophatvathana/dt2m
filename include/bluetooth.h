/*
  Bluetooth Module for ESP32 dt2m
  Handles BLE jamming, scanning, and spoofing functionality
  
  Created for better code organization and maintainability
*/

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>
#include <nRF24L01.h>
#include <RF24.h>

// Bluetooth jammer modes
enum JammerMode {
  BLE_MODE,
  BLUETOOTH_MODE, 
  ALL_MODE
};

// Bluetooth class to handle all Bluetooth operations
class BluetoothModule {
private:
  // Radio objects
  RF24* radio1;
  RF24* radio2;
  RF24* radio3;
  
  // Jammer state
  bool jammerActive;
  bool jammerPaused;
  bool radio1Active;
  bool radio2Active;
  bool radio3Active;
  
  // Power management
  rf24_pa_dbm_e radio1PowerLevel;
  rf24_pa_dbm_e radio2PowerLevel;
  rf24_pa_dbm_e radio3PowerLevel;
  bool powerRampingComplete;
  unsigned long lastPowerIncreaseTime;
  
  // Channel hopping
  unsigned long lastChannelChange;
  const unsigned long channelChangeInterval = 0; // 5ms for ULTRA FAST jamming
  
  // Current mode
  JammerMode currentJammerMode;
  const char* jammerModeNames[3] = {"ALL", "BLE", "Bluetooth"};
  
  // Channel arrays
  static const byte BLE_channels[];
  static const byte channelGroup1[];
  static const byte channelGroup2[];
  static const byte channelGroup3[];
  static const int all_channels[];
  static const int ble_channels[];
  static const int bluetooth_channels[];
  
  // Private methods
  bool initializeRadioWithRetry(RF24 &radio, const byte* channels, size_t size, const char* radioName, int maxRetries = 10, int retryDelay = 500);
  void configureRadio(RF24 &radio, const byte* channels, size_t size, rf24_pa_dbm_e powerLevel);
  void verifyJammerStatus();
  
public:
  // Constructor
  BluetoothModule(RF24* r1, RF24* r2, RF24* r3);
  
  // Jammer control
  void startJammer();
  void stopJammer();
  void pauseJammer();
  void resumeJammer();
  void restartJammer();
  void forceRestartJammer();
  void testJammer();
  void resetESP32RadioState();
  void updateJammer();
  void cycleMode();
  
  // Status and control
  bool isJammerActive() const { return jammerActive; }
  bool isJammerPaused() const { return jammerPaused; }
  JammerMode getCurrentMode() const { return currentJammerMode; }
  const char* getModeName() const { return jammerModeNames[currentJammerMode]; }
  
  // Radio status
  bool isRadio1Active() const { return radio1Active; }
  bool isRadio2Active() const { return radio2Active; }
  bool isRadio3Active() const { return radio3Active; }
  
  // Power level
  rf24_pa_dbm_e getPowerLevel() const { return radio1PowerLevel; }
  void setMaxPower();
  void cyclePowerLevel();
  void turboBoost(); // Immediate MAX power for all radios
  
  // BLE features
  void runBleSpoofer();
  void runBleScanner();
  void runBleJammer();
  
  // Utility
  void powerDownAllRadios();
  void ensureCC1101Off();
  void disableBluetooth();
  void disableWiFi();
};

// Global Bluetooth module instance (will be defined in main.cpp)
extern BluetoothModule* bluetoothModule;

#endif // BLUETOOTH_H
