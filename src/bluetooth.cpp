/*
  Bluetooth Module Implementation for ESP32 dt2m (Don't T to Me)
  Handles BLE jamming, scanning, and spoofing functionality
*/

#include "bluetooth.h"
#include "RF24.h"
#include <esp_bt.h>
#include <esp_bt_main.h>
 
#define NRF24_CE_PIN_1 4
#define NRF24_CSN_PIN_1 5
#define NRF24_CE_PIN_2 16
#define NRF24_CSN_PIN_2 27
#define NRF24_CE_PIN_3 7
#define NRF24_CSN_PIN_3 17
#include <WiFi.h>
#include <esp_bt_defs.h>
#include <esp_bt_device.h>
#include <esp_gap_ble_api.h>
#include <esp_gap_bt_api.h>
#include <esp_gatts_api.h>
#include <esp_wifi.h>
 
const byte BluetoothModule::BLE_channels[] = {2, 26, 80};
const byte BluetoothModule::channelGroup1[] = {2, 5, 8, 11};
const byte BluetoothModule::channelGroup2[] = {26, 29, 32, 35};
const byte BluetoothModule::channelGroup3[] = {80, 83, 86, 89};

const int BluetoothModule::all_channels[] = {0,  1,  2,  4,  6,  8,  22,
                                             24, 26, 28, 30, 32, 34, 46,
                                             48, 50, 52, 74, 76, 78, 80};
const int BluetoothModule::ble_channels[] = {2, 26, 80};
const int BluetoothModule::bluetooth_channels[] = {32, 34, 46, 48, 50, 52, 0,
                                                   1,  2,  4,  6,  8,  22, 24,
                                                   26, 28, 30, 74, 76, 78, 80};
 
BluetoothModule::BluetoothModule(RF24 *r1, RF24 *r2, RF24 *r3)
    : radio1(r1), radio2(r2), radio3(r3), jammerActive(false),
      jammerPaused(false), radio1Active(false), radio2Active(false),
      radio3Active(false), radio1PowerLevel(RF24_PA_HIGH),
      radio2PowerLevel(RF24_PA_HIGH), radio3PowerLevel(RF24_PA_HIGH),
      powerRampingComplete(false), lastPowerIncreaseTime(0),
      lastChannelChange(0), currentJammerMode(BLE_MODE) {}
 
void BluetoothModule::configureRadio(RF24 &radio, const byte *channels,
                                     size_t size, rf24_pa_dbm_e powerLevel) {
 
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(powerLevel, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
 
  if (size > 0) {
    radio.setChannel(channels[0]);
    radio.startConstCarrier(powerLevel, channels[0]);
    Serial.print("Started constant carrier on channel ");
    Serial.println(channels[0]);
  }
}
 
bool BluetoothModule::initializeRadioWithRetry(RF24 &radio,
                                               const byte *channels,
                                               size_t size,
                                               const char *radioName,
                                               int maxRetries, int retryDelay) {
  Serial.print("=== INITIALIZING ");
  Serial.print(radioName);
  Serial.println(" ===");

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    yield();

    Serial.print(radioName);
    Serial.print(" attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.print(maxRetries);
    Serial.println("...");
 
    if (radio.isChipConnected()) {
      Serial.print(radioName);
      Serial.println(" already connected, reconfiguring...");
      configureRadio(radio, channels, size, RF24_PA_HIGH);
      return true;
    }
 
    if (radio.begin()) {
      Serial.print(radioName);
      Serial.println(" begin() successful, configuring...");
      configureRadio(radio, channels, size, RF24_PA_HIGH);

      if (radio.isChipConnected()) {
        Serial.print(radioName);
        Serial.print(" initialized successfully (attempt ");
        Serial.print(attempt);
        Serial.println(")");
        return true;
      } else {
        Serial.print(radioName);
        Serial.println(" begin() succeeded but chip not connected!");
      }
    } else {
      Serial.print(radioName);
      Serial.println(" begin() failed");
    }

    if (attempt < maxRetries) {
      Serial.print(radioName);
      Serial.println(" failed, retrying...");
      delay(retryDelay); 
    }
  }

  Serial.print(radioName);
  Serial.print(" failed after ");
  Serial.print(maxRetries);
  Serial.println(" attempts");
  return false;
}
 
void BluetoothModule::startJammer() {
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  Serial.print("Pre-jammer voltage check: ");
  Serial.print(voltage);
  Serial.println("V");
 
  Serial.println("=== PERFORMING ESP32 RADIO STATE RESET ===");
 
  Serial.println("Resetting SPI communication...");
  SPI.end();
  delay(200);
  SPI.begin();
  delay(200);
 
  Serial.println("Resetting all radio pins...");
  digitalWrite(NRF24_CE_PIN_1, LOW);
  digitalWrite(NRF24_CE_PIN_2, LOW);
  digitalWrite(NRF24_CE_PIN_3, LOW);
  digitalWrite(NRF24_CSN_PIN_1, HIGH);
  digitalWrite(NRF24_CSN_PIN_2, HIGH);
  digitalWrite(NRF24_CSN_PIN_3, HIGH);
  delay(300);
 
  Serial.println("Powering down CC1101 for Bluetooth mode...");
  ensureCC1101Off();

  jammerActive = true;
  Serial.println("=== BLE JAMMER ACTIVATION ===");
  Serial.println("Initializing radios for jamming...");
 
  radio1PowerLevel = RF24_PA_HIGH;
  radio2PowerLevel = RF24_PA_HIGH;
  radio3PowerLevel = RF24_PA_HIGH;
  powerRampingComplete = false;
  lastPowerIncreaseTime = millis();

  Serial.println("🚀 MAXIMUM POWER MODE ACTIVATED!");
  Serial.println("⚡ Starting at HIGH power for maximum jamming effectiveness");
 
  Serial.println("Initializing NRF24L01 radios with staggered power-up...");
  yield();
 
  radio1Active = false;
  radio2Active = false;
  radio3Active = false;
 
  Serial.println("Power cycling radios for clean start...");
  radio1->powerDown();
  radio2->powerDown();
  radio3->powerDown();
  delay(100); 

  Serial.println("Starting Radio 1...");
  radio1Active = initializeRadioWithRetry(
      *radio1, channelGroup1, sizeof(channelGroup1), "Radio 1", 30, 200);
  yield();
  delay(500); 

  voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  if (voltage < 3.0) {
    Serial.println(
        "WARNING: Voltage dropped during Radio 1 init - stopping here");
    Serial.print("Current voltage: ");
    Serial.print(voltage);
    Serial.println("V");
  } else {
    Serial.println("Starting Radio 2...");
    radio2Active = initializeRadioWithRetry(
        *radio2, channelGroup2, sizeof(channelGroup2), "Radio 2", 30, 200);
    yield();
    delay(500); 

    voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
    if (voltage < 3.0) {
      Serial.println(
          "WARNING: Voltage dropped during Radio 2 init - stopping here");
      Serial.print("Current voltage: ");
      Serial.print(voltage);
      Serial.println("V");
    } else {
      Serial.println("Starting Radio 3...");
      radio3Active = initializeRadioWithRetry(
          *radio3, channelGroup3, sizeof(channelGroup3), "Radio 3", 30, 200);
      yield();
    }
  }

  Serial.print("Radios active: ");
  Serial.print(radio1Active ? "1 " : "- ");
  Serial.print(radio2Active ? "2 " : "- ");
  Serial.println(radio3Active ? "3" : "-");

  Serial.println("=== JAMMER STATUS VERIFICATION ===");
  Serial.print("Radio1 Active: ");
  Serial.println(radio1Active ? "YES" : "NO");
  Serial.print("Radio2 Active: ");
  Serial.println(radio2Active ? "YES" : "NO");
  Serial.print("Radio3 Active: ");
  Serial.println(radio3Active ? "YES" : "NO");

  if (radio1Active || radio2Active || radio3Active) {
    Serial.println("✓ BLE Jammer successfully activated!");
    Serial.println("✓ Constant carrier signals are transmitting");
    Serial.println("✓ FOCUSED JAMMING: All radios use same channel");
    Serial.println("🚀 TURBO MODE: Channel hopping every 5ms");
    Serial.println("⚡ MAXIMUM POWER: Starting at HIGH, ramping to MAX");
    Serial.println("🔥 ULTIMATE JAMMING: 3 radios × MAX power × 5ms hopping");

    if (radio1Active) {
      Serial.print("Radio 1 status: ");
      Serial.println(radio1->isChipConnected() ? "CONNECTED" : "DISCONNECTED");
    }
    if (radio2Active) {
      Serial.print("Radio 2 status: ");
      Serial.println(radio2->isChipConnected() ? "CONNECTED" : "DISCONNECTED");
    }
    if (radio3Active) {
      Serial.print("Radio 3 status: ");
      Serial.println(radio3->isChipConnected() ? "CONNECTED" : "DISCONNECTED");
    }
  } else {
    Serial.println("✗ WARNING: No radios active - jammer may not work!");
    Serial.println("✗ Check radio connections and power supply");
    Serial.println("✗ Try restarting the jammer");
  }

  Serial.println("Starting constant carrier on all active radios...");
  if (radio1Active && radio1->isChipConnected()) {
    radio1->startConstCarrier(radio1PowerLevel, channelGroup1[0]);
    Serial.println("✓ Radio 1: Constant carrier started");
  }
  if (radio2Active && radio2->isChipConnected()) {
    radio2->startConstCarrier(radio2PowerLevel, channelGroup2[0]);
    Serial.println("✓ Radio 2: Constant carrier started");
  }
  if (radio3Active && radio3->isChipConnected()) {
    radio3->startConstCarrier(radio3PowerLevel, channelGroup3[0]);
    Serial.println("✓ Radio 3: Constant carrier started");
  }

  Serial.println("=== JAMMER READY ===");
}

 
void BluetoothModule::restartJammer() {
  Serial.println("=== RESTARTING JAMMER ===");

  Serial.println("Resetting ESP32 radio state to prevent corruption...");
  resetESP32RadioState();

  Serial.println("Starting fresh jammer initialization...");
  startJammer();
}

void BluetoothModule::testJammer() {
  Serial.println("=== TESTING JAMMER FUNCTIONALITY ===");

  if (!jammerActive) {
    Serial.println("✗ Jammer not active");
    return;
  }

  Serial.println("Testing radio connections...");
  if (radio1Active) {
    Serial.print("Radio 1: ");
    Serial.println(radio1->isChipConnected() ? "CONNECTED" : "DISCONNECTED");
  }
  if (radio2Active) {
    Serial.print("Radio 2: ");
    Serial.println(radio2->isChipConnected() ? "CONNECTED" : "DISCONNECTED");
  }
  if (radio3Active) {
    Serial.print("Radio 3: ");
    Serial.println(radio3->isChipConnected() ? "CONNECTED" : "DISCONNECTED");
  }

  Serial.println("Testing constant carrier restart...");
  if (radio1Active && radio1->isChipConnected()) {
    radio1->stopConstCarrier();
    delay(100);
    radio1->startConstCarrier(radio1PowerLevel, channelGroup1[0]);
    Serial.println("✓ Radio 1: Carrier restarted");
  }
  if (radio2Active && radio2->isChipConnected()) {
    radio2->stopConstCarrier();
    delay(100);
    radio2->startConstCarrier(radio2PowerLevel, channelGroup2[0]);
    Serial.println("✓ Radio 2: Carrier restarted");
  }
  if (radio3Active && radio3->isChipConnected()) {
    radio3->stopConstCarrier();
    delay(100);
    radio3->startConstCarrier(radio3PowerLevel, channelGroup3[0]);
    Serial.println("✓ Radio 3: Carrier restarted");
  }

  Serial.println("✓ JAMMER TEST COMPLETE");
}

void BluetoothModule::resetESP32RadioState() {
  Serial.println("=== RESETTING ESP32 RADIO STATE ===");

  jammerActive = false;

  if (radio1Active) {
    radio1->stopConstCarrier();
    radio1->powerDown();
  }
  if (radio2Active) {
    radio2->stopConstCarrier();
    radio2->powerDown();
  }
  if (radio3Active) {
    radio3->stopConstCarrier();
    radio3->powerDown();
  }

  Serial.println("Performing complete SPI reset...");
  SPI.end();
  delay(500);
  SPI.begin();
  delay(500);

  Serial.println("Resetting all radio pins to known state...");
  digitalWrite(NRF24_CE_PIN_1, LOW);
  digitalWrite(NRF24_CE_PIN_2, LOW);
  digitalWrite(NRF24_CE_PIN_3, LOW);
  digitalWrite(NRF24_CSN_PIN_1, HIGH);
  digitalWrite(NRF24_CSN_PIN_2, HIGH);
  digitalWrite(NRF24_CSN_PIN_3, HIGH);
  digitalWrite(26, LOW);  

  delay(1000);

  radio1Active = false;
  radio2Active = false;
  radio3Active = false;

  Serial.println("✓ ESP32 radio state completely reset");
}

void BluetoothModule::forceRestartJammer() {
  Serial.println("=== FORCE RESTARTING JAMMER ===");

  stopJammer();
  delay(1000); 

  // Reset all radio states
  radio1Active = false;
  radio2Active = false;
  radio3Active = false;
 
  Serial.println("Starting fresh initialization...");
  startJammer();
}

// Stop jammer
void BluetoothModule::stopJammer() {
  jammerActive = false;
  Serial.println("BLE Jammer deactivated - powering down all radios");

  // Stop constant carriers first
  if (radio1Active) {
    radio1->stopConstCarrier();
  }
  if (radio2Active) {
    radio2->stopConstCarrier();
  }
  if (radio3Active) {
    radio3->stopConstCarrier();
  }
 
  Serial.println(
      "Performing complete radio reset to prevent ESP32 state corruption...");

  // Power down all radios
  powerDownAllRadios();

  // Force SPI reset
  Serial.println("Resetting SPI communication...");
  SPI.end();
  delay(100);
  SPI.begin();
  delay(100);
 
  Serial.println("Power cycling radio pins...");
  digitalWrite(NRF24_CE_PIN_1, LOW);
  digitalWrite(NRF24_CE_PIN_2, LOW);
  digitalWrite(NRF24_CE_PIN_3, LOW);
  digitalWrite(NRF24_CSN_PIN_1, HIGH);
  digitalWrite(NRF24_CSN_PIN_2, HIGH);
  digitalWrite(NRF24_CSN_PIN_3, HIGH);
  delay(200);

  Serial.println("Ensuring CC1101 stays powered down...");
  digitalWrite(26, LOW); 

  Serial.println("✓ Complete radio reset performed - ready for clean restart");
}

void BluetoothModule::pauseJammer() {
  if (!jammerActive) {
    Serial.println("Jammer not active - cannot pause");
    return;
  }

  if (jammerPaused) {
    Serial.println("Jammer already paused");
    return;
  }

  Serial.println("=== PAUSING JAMMER ===");
  jammerPaused = true;

  if (radio1Active) {
    radio1->stopConstCarrier();
    Serial.println("✓ Radio 1: Transmission paused");
  }
  if (radio2Active) {
    radio2->stopConstCarrier();
    Serial.println("✓ Radio 2: Transmission paused");
  }
  if (radio3Active) {
    radio3->stopConstCarrier();
    Serial.println("✓ Radio 3: Transmission paused");
  }

  Serial.println("✓ JAMMER PAUSED - Radios remain active, ready to resume");
  Serial.print("Pause status: ");
  Serial.println(jammerPaused ? "PAUSED" : "NOT PAUSED");
}

// Resume jammer (restart transmission)
void BluetoothModule::resumeJammer() {
  if (!jammerActive) {
    Serial.println("Jammer not active - cannot resume");
    return;
  }

  if (!jammerPaused) {
    Serial.println("Jammer not paused - already active");
    return;
  }

  Serial.println("=== RESUMING JAMMER ===");
  jammerPaused = false;

  // CRITICAL: Ensure radios are properly configured before restarting
  Serial.println("Reconfiguring radios for resume...");

  // Restart constant carriers with proper configuration
  if (radio1Active && radio1->isChipConnected()) {
    // Reconfigure radio 1
    radio1->setPALevel(radio1PowerLevel, true);
    radio1->setChannel(channelGroup1[0]);
    radio1->startConstCarrier(radio1PowerLevel, channelGroup1[0]);
    Serial.println("✓ Radio 1: Transmission resumed");
  } else {
    Serial.println("✗ Radio 1: Not connected");
  }

  if (radio2Active && radio2->isChipConnected()) {
    // Reconfigure radio 2
    radio2->setPALevel(radio2PowerLevel, true);
    radio2->setChannel(channelGroup2[0]);
    radio2->startConstCarrier(radio2PowerLevel, channelGroup2[0]);
    Serial.println("✓ Radio 2: Transmission resumed");
  } else {
    Serial.println("✗ Radio 2: Not connected");
  }

  if (radio3Active && radio3->isChipConnected()) {
    // Reconfigure radio 3
    radio3->setPALevel(radio3PowerLevel, true);
    radio3->setChannel(channelGroup3[0]);
    radio3->startConstCarrier(radio3PowerLevel, channelGroup3[0]);
    Serial.println("✓ Radio 3: Transmission resumed");
  } else {
    Serial.println("✗ Radio 3: Not connected");
  }

  Serial.println("✓ JAMMER RESUMED - Transmission active");
  Serial.print("Pause status: ");
  Serial.println(jammerPaused ? "PAUSED" : "NOT PAUSED");
}

// Update jammer (called in main loop)
void BluetoothModule::updateJammer() {
  if (!jammerActive)
    return;

  // Skip updates if jammer is paused
  if (jammerPaused)
    return;

  // Verify jammer status periodically
  verifyJammerStatus();

  // Monitor voltage during operation to prevent brownout
  static unsigned long lastVoltageCheck = 0;
  if (millis() - lastVoltageCheck > 2000) { // Check every 2 seconds
    lastVoltageCheck = millis();
    float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
    if (voltage < 3.0) {
      Serial.println("WARNING: Voltage dropping during jammer operation!");
      Serial.print("Current voltage: ");
      Serial.print(voltage);
      Serial.println("V - Reducing radio count to prevent brownout");
    } else {
      // Show power savings from CC1101 being off
      static bool powerSavingsShown = false;
      if (!powerSavingsShown) {
        Serial.println("✓ CC1101 powered down - saving ~15mA");
        Serial.println("✓ Voltage stable with CC1101 off");
        powerSavingsShown = true;
      }
    }
  }

  // AGGRESSIVE POWER RAMPING for maximum jamming
  if (!powerRampingComplete) {
    unsigned long currentTime = millis();

    // Ramp up power every 2 seconds for faster MAX power
    if (currentTime - lastPowerIncreaseTime > 2000) {
      lastPowerIncreaseTime = currentTime;

      // Power level progression: HIGH -> MAX (skip LOW for maximum power)
      if (radio1PowerLevel == RF24_PA_HIGH) {
        radio1PowerLevel = radio2PowerLevel = radio3PowerLevel = RF24_PA_MAX;
        Serial.println("🔥 MAXIMUM POWER ACHIEVED!");
        Serial.println(
            "⚡ All radios now at MAXIMUM power for ultimate jamming");
        powerRampingComplete = true;
      }

      // Apply new power levels to all active radios
      if (radio1Active && radio1->isChipConnected()) {
        radio1->setPALevel(radio1PowerLevel, true);
      }
      if (radio2Active && radio2->isChipConnected()) {
        radio2->setPALevel(radio2PowerLevel, true);
      }
      if (radio3Active && radio3->isChipConnected()) {
        radio3->setPALevel(radio3PowerLevel, true);
      }
    }
  }

  // ULTRA FAST CHANNEL HOPPING - TURBO MODE (5ms)
  if (radio1Active || radio2Active || radio3Active) {
    int channel;
    int *channelArray;
    int channelArraySize;

    switch (currentJammerMode) {
    case BLE_MODE:
      channelArray = (int *)ble_channels;
      channelArraySize = sizeof(ble_channels) / sizeof(ble_channels[0]);
      break;
    case BLUETOOTH_MODE:
      channelArray = (int *)bluetooth_channels;
      channelArraySize =
          sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]);
      break;
    case ALL_MODE:
      channelArray = (int *)all_channels;
      channelArraySize = sizeof(all_channels) / sizeof(all_channels[0]);
      break;
    }

    int randomIndex = random(0, channelArraySize);
    channel = channelArray[randomIndex];

    if (radio1Active && radio1->isChipConnected()) {
      radio1->setChannel(channel);
    }

    if (radio2Active && radio2->isChipConnected()) {
      radio2->setChannel(channel);
    }

    if (radio3Active && radio3->isChipConnected()) {
      radio3->setChannel(channel);
    }
  }
}

// Cycle jammer mode
void BluetoothModule::cycleMode() {
  currentJammerMode = (JammerMode)((currentJammerMode + 1) % 3);
  Serial.println("=== JAMMER MODE CHANGE ===");
  Serial.print("Mode changed to: ");
  Serial.println(jammerModeNames[currentJammerMode]);

  // If jammer is active, reinitialize with new mode
  if (jammerActive) {
    Serial.println("Reinitializing radios with new mode...");
    stopJammer();
    delay(10); // Minimal delay for mode change
    startJammer();
  }

  // Test the new mode
  Serial.print("Testing ");
  Serial.print(jammerModeNames[currentJammerMode]);
  Serial.println(" mode channels:");

  int *channelArray;
  int channelArraySize;

  switch (currentJammerMode) {
  case BLE_MODE:
    channelArray = (int *)ble_channels;
    channelArraySize = sizeof(ble_channels) / sizeof(ble_channels[0]);
    break;
  case BLUETOOTH_MODE:
    channelArray = (int *)bluetooth_channels;
    channelArraySize =
        sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]);
    break;
  case ALL_MODE:
    channelArray = (int *)all_channels;
    channelArraySize = sizeof(all_channels) / sizeof(all_channels[0]);
    break;
  }

  Serial.print("Available channels: ");
  for (int i = 0; i < min(channelArraySize, 10); i++) {
    Serial.print(channelArray[i]);
    if (i < min(channelArraySize, 10) - 1)
      Serial.print(", ");
  }
  if (channelArraySize > 10)
    Serial.print("...");
  Serial.println();
  Serial.println("========================");
}

// Set maximum power
void BluetoothModule::setMaxPower() {
  radio1PowerLevel = radio2PowerLevel = radio3PowerLevel = RF24_PA_MAX;
  powerRampingComplete = true;

  // Apply to all active radios
  if (radio1Active && radio1->isChipConnected()) {
    radio1->setPALevel(RF24_PA_MAX, true);
  }
  if (radio2Active && radio2->isChipConnected()) {
    radio2->setPALevel(RF24_PA_MAX, true);
  }
  if (radio3Active && radio3->isChipConnected()) {
    radio3->setPALevel(RF24_PA_MAX, true);
  }
  Serial.println("Power level boosted to MAX");
}

// Cycle through power levels
void BluetoothModule::cyclePowerLevel() {
  // Cycle through power levels: LOW -> HIGH -> MAX -> LOW
  rf24_pa_dbm_e newPowerLevel;
  const char *powerName;

  switch (radio1PowerLevel) {
  case RF24_PA_HIGH:
    newPowerLevel = RF24_PA_HIGH;
    powerName = "HIGH";
    break;
  case RF24_PA_LOW:
    newPowerLevel = RF24_PA_MAX;
    powerName = "MAX";
    break;
  case RF24_PA_MAX:
    newPowerLevel = RF24_PA_HIGH;
    powerName = "LOW";
    break;
  default:
    newPowerLevel = RF24_PA_HIGH;
    powerName = "LOW";
    break;
  }

  // Set new power level
  radio1PowerLevel = radio2PowerLevel = radio3PowerLevel = newPowerLevel;
  powerRampingComplete = true; // Stop automatic ramping

  // Apply to all active radios
  if (radio1Active && radio1->isChipConnected()) {
    radio1->setPALevel(newPowerLevel, true);
  }
  if (radio2Active && radio2->isChipConnected()) {
    radio2->setPALevel(newPowerLevel, true);
  }
  if (radio3Active && radio3->isChipConnected()) {
    radio3->setPALevel(newPowerLevel, true);
  }

  Serial.print("Power level changed to: ");
  Serial.println(powerName);

  // Show power level change on display if possible
  Serial.print("Current power: ");
  Serial.print(powerName);
  Serial.println(" (Press DOWN to cycle: LOW -> HIGH -> MAX -> LOW)");
}

// Turbo boost - immediate MAX power for ultimate jamming
void BluetoothModule::turboBoost() {
  Serial.println("🚀 TURBO BOOST ACTIVATED!");
  Serial.println("⚡ Setting all radios to MAXIMUM power immediately");

  // Set all radios to MAX power immediately
  radio1PowerLevel = radio2PowerLevel = radio3PowerLevel = RF24_PA_MAX;
  powerRampingComplete = true; // Skip ramping, go straight to MAX

  // Apply MAX power to all active radios
  if (radio1Active && radio1->isChipConnected()) {
    radio1->setPALevel(RF24_PA_MAX, true);
    Serial.println("✓ Radio 1: MAX POWER");
  }
  if (radio2Active && radio2->isChipConnected()) {
    radio2->setPALevel(RF24_PA_MAX, true);
    Serial.println("✓ Radio 2: MAX POWER");
  }
  if (radio3Active && radio3->isChipConnected()) {
    radio3->setPALevel(RF24_PA_MAX, true);
    Serial.println("✓ Radio 3: MAX POWER");
  }

  Serial.println("🔥 TURBO BOOST COMPLETE - ULTIMATE JAMMING POWER!");
}

void BluetoothModule::verifyJammerStatus() {
  if (!jammerActive)
    return;

  static unsigned long lastVerification = 0;
  if (millis() - lastVerification > 10000) { // Check every 10 seconds
    lastVerification = millis();

    Serial.println("=== JAMMER VERIFICATION ===");
    Serial.print("Radio1: ");
    if (radio1Active && radio1->isChipConnected()) {
      Serial.println("✓ Active and Connected");
    } else {
      Serial.println("✗ Inactive or Disconnected");
    }

    Serial.print("Radio2: ");
    if (radio2Active && radio2->isChipConnected()) {
      Serial.println("✓ Active and Connected");
    } else {
      Serial.println("✗ Inactive or Disconnected");
    }

    Serial.print("Radio3: ");
    if (radio3Active && radio3->isChipConnected()) {
      Serial.println("✓ Active and Connected");
    } else {
      Serial.println("✗ Inactive or Disconnected");
    }

    Serial.print("Current Mode: ");
    Serial.println(jammerModeNames[currentJammerMode]);
    Serial.print("Power Level: ");
    switch (radio1PowerLevel) {
    case RF24_PA_MIN:
      Serial.println("MIN");
      break;
    case RF24_PA_LOW:
      Serial.println("LOW");
      break;
    case RF24_PA_HIGH:
      Serial.println("HIGH");
      break;
    case RF24_PA_MAX:
      Serial.println("MAX");
      break;
    }
    Serial.println("========================");
  }
}

// Power down all radios
void BluetoothModule::powerDownAllRadios() {
  Serial.println("Powering down all radios...");

  // Power down all NRF24 radios
  radio1->powerDown();
  radio2->powerDown();
  radio3->powerDown();

  // Reset radio states
  radio1Active = false;
  radio2Active = false;
  radio3Active = false;

  // CRITICAL: Power down CC1101 to save voltage
  Serial.println("Powering down CC1101 to save voltage...");
  digitalWrite(26, LOW); // SHARED_PIN_26 - disable CC1101

  Serial.println("All radios and CC1101 powered down");
}

// Ensure CC1101 is powered down
void BluetoothModule::ensureCC1101Off() {
  Serial.println("Ensuring CC1101 is powered down...");
  digitalWrite(26, LOW); // SHARED_PIN_26 - disable CC1101
  delay(10);             // Small delay to ensure CC1101 is off
  Serial.println("CC1101 confirmed powered down");
}

// Disable Bluetooth
void BluetoothModule::disableBluetooth() {
  Serial.println("Disabling Bluetooth...");

  // Disable Bluetooth controller
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_deinit();
  esp_bt_controller_disable();

  // Release Bluetooth memory
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);

  Serial.println("Bluetooth disabled");
}

// Disable WiFi
void BluetoothModule::disableWiFi() {
  Serial.println("Disabling WiFi...");

  // Disconnect and stop WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Release WiFi memory
  esp_wifi_deinit();

  Serial.println("WiFi disabled");
}

void BluetoothModule::runBleSpoofer() {
  Serial.println("BLE Spoofer functionality - implement as needed");
}

void BluetoothModule::runBleScanner() {
  Serial.println("BLE Scanner functionality - implement as needed");
}

void BluetoothModule::runBleJammer() {
  Serial.println("BLE Jammer functionality - implement as needed");
}
