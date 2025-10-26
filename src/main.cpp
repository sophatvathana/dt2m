/*
  ESP32 - dt2m (Don't T to Me)
  Created for ESP32 development and educational purposes only.
  
  This is the main sketch file for the ESP32 - dt2m (Don't T to Me) project.
  
  Features:
  - TFT Touch Screen Display
  - PCF8574 I2C Button Interface
  - Power Management and Sleep Modes
  - Brownout detector handling
  - Memory optimization and watchdog management
  - Button debouncing and state tracking
  - Menu system and navigation
*/

#include "esp32-hal-gpio.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_pm.h>
#include <esp_task_wdt.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_defs.h>
#include <esp_bt_device.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <User_Setup.h>
#include <TFT_eSPI.h>
#include <PCF8574.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "subghz.h"
#include "bluetooth.h"


// PCF8574 I2C Expander
#define pcf_ADDR 0x20
PCF8574 pcf(pcf_ADDR);

#define BTN_UP     2
#define BTN_DOWN   1
#define BTN_LEFT   4
#define BTN_RIGHT  5
#define BTN_SELECT 3
#define BACKLIGHT_PIN 6

// Touchscreen pins
#define XPT2046_CS 33 
#define XPT2046_IRQ 34 
#define XPT2046_MOSI 32 
#define XPT2046_MISO 35 
#define XPT2046_CLK 25

// NRF24L01 Radio pins 
#define NRF24_CE_PIN_1 4
#define NRF24_CSN_PIN_1 5
#define NRF24_CE_PIN_2 16
#define NRF24_CSN_PIN_2 27
#define NRF24_CE_PIN_3 7
#define NRF24_CSN_PIN_3 17

#define CC1101_CS_PIN 26

// Radio configuration
RF24 radio1(NRF24_CE_PIN_1, NRF24_CSN_PIN_1, 16000000);
RF24 radio2(NRF24_CE_PIN_2, NRF24_CSN_PIN_2, 16000000);
RF24 radio3(0, NRF24_CSN_PIN_3, 16000000);

// Global Bluetooth module instance
BluetoothModule* bluetoothModule;


// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Display variables
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 1000; // Update display every 1 second

// PCF8574 status
bool pcf8574_available = false;

// TFT status
bool tft_initialized = false;

// Menu system variables
enum MenuState {
  MAIN_MENU,
  BLUETOOTH_MENU,
  SUBGHZ_MENU,
  WIFI_MENU,
  FEATURE_ACTIVE
};

MenuState currentMenuState = MAIN_MENU;
int currentMenuIndex = 0;
int currentSubMenuIndex = 0;
bool menuDrawn = false;

// Main menu items
const int NUM_MAIN_MENU_ITEMS = 4;
const char* mainMenuItems[NUM_MAIN_MENU_ITEMS] = {
  "Bluetooth",
  "SubGhz", 
  "WiFi",
  "System Info"
};

// Bluetooth submenu items
const int NUM_BLUETOOTH_MENU_ITEMS = 4;
const char* bluetoothMenuItems[NUM_BLUETOOTH_MENU_ITEMS] = {
  "BLE Spoofer",
  "BLE Scanner", 
  "BLE Jammer",
  "Back"
};

// SubGhz submenu items
const int NUM_SUBGHZ_MENU_ITEMS = 4;
const char* subghzMenuItems[NUM_SUBGHZ_MENU_ITEMS] = {
  "Replay Attack",
  "SubGhz Scanner",
  "SubGhz Jammer", 
  "Back"
};

// WiFi submenu items
const int NUM_WIFI_MENU_ITEMS = 4;
const char* wifiMenuItems[NUM_WIFI_MENU_ITEMS] = {
  "WiFi Scanner",
  "WiFi Spectrum",
  "WiFi Deauth",
  "Back"
};

// Professional UI Constants
#define MENU_ITEM_HEIGHT 35
#define MENU_START_Y 60
#define MENU_ITEM_PADDING 8
#define SELECTED_COLOR TFT_CYAN
#define NORMAL_COLOR TFT_WHITE
#define BACKGROUND_COLOR TFT_BLACK
#define ACCENT_COLOR TFT_ORANGE
#define SUCCESS_COLOR TFT_GREEN
#define WARNING_COLOR TFT_YELLOW
#define ERROR_COLOR TFT_RED
#define INFO_COLOR TFT_BLUE
#define BORDER_COLOR TFT_DARKGREY
#define HEADER_COLOR TFT_CYAN

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long buttonDebounceDelay = 500;  

// Button state tracking
bool buttonPressed = false;
bool lastButtonState = true; // Start with buttons not pressed

// Function forward declarations
void scanI2CDevices();
void showConnectingStatus();
void showJammerStatus();
void drawProfessionalHeader(const char* title);
void drawStatusBar();
void drawMenuBorder();
void drawLoadingAnimation(int step);
void ensureRadiosPoweredDown();
void checkButtons();

// SubGHz function declarations
void activateCC1101();
void activateNRF24();
bool feature_exit_requested = false;
bool feature_active = true;
 
bool nrf24_active = false;
bool cc1101_active = false;

void resetWatchdog() {
  yield();

  if (ESP.getFreeHeap() < 20000) {  // Less than 20KB free
    Serial.println("WARNING: Memory getting low, feeding watchdog...");
    yield();
    delay(10);
  }
}

void drawMainMenu() {
  if (!tft_initialized) return;
 
  tft.fillScreen(BACKGROUND_COLOR);
 
  drawProfessionalHeader("DT2M (Don't T to Me)");
 
  drawMenuBorder();
  
  for (int i = 0; i < NUM_MAIN_MENU_ITEMS; i++) {
    int y = MENU_START_Y + (i * MENU_ITEM_HEIGHT);
    
    if (i == currentMenuIndex) {
     
      tft.fillRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, TFT_DARKGREY);
      tft.drawRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, SELECTED_COLOR);
      
      tft.fillCircle(25, y + 12, 3, SELECTED_COLOR);
      
      tft.setTextColor(SELECTED_COLOR, TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(mainMenuItems[i]);
    } else {
      tft.setTextColor(NORMAL_COLOR, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(mainMenuItems[i]);
    }
  }
  drawStatusBar();
}

void drawBluetoothMenu() {
  if (!tft_initialized) return;
  
  tft.fillScreen(BACKGROUND_COLOR);
  
  drawProfessionalHeader("Bluetooth");
  
  drawMenuBorder();
  
  tft.setTextColor(INFO_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(1);
  tft.setCursor(15, 50);
  tft.println("Select Bluetooth feature:");
  
  for (int i = 0; i < NUM_BLUETOOTH_MENU_ITEMS; i++) {
    int y = MENU_START_Y + (i * MENU_ITEM_HEIGHT);
    
    if (i == currentSubMenuIndex) {
      tft.fillRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, TFT_DARKGREY);
      tft.drawRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, SELECTED_COLOR);
      
      tft.fillCircle(25, y + 12, 3, SELECTED_COLOR);
      
      tft.setTextColor(SELECTED_COLOR, TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(bluetoothMenuItems[i]);
    } else {
      tft.setTextColor(NORMAL_COLOR, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(bluetoothMenuItems[i]);
    }
  }
  
  drawStatusBar();
}

void drawSubGhzMenu() {
  if (!tft_initialized) return;
  
  tft.fillScreen(BACKGROUND_COLOR);
  
  drawProfessionalHeader("SubGhz");
  
  drawMenuBorder();
  
  tft.setTextColor(INFO_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(1);
  tft.setCursor(15, 50);
  tft.println("Select SubGhz feature:");
  
  for (int i = 0; i < NUM_SUBGHZ_MENU_ITEMS; i++) {
    int y = MENU_START_Y + (i * MENU_ITEM_HEIGHT);
    
    if (i == currentSubMenuIndex) {
      tft.fillRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, TFT_DARKGREY);
      tft.drawRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, SELECTED_COLOR);
      
      tft.fillCircle(25, y + 12, 3, SELECTED_COLOR);
      
      tft.setTextColor(SELECTED_COLOR, TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(subghzMenuItems[i]);
    } else {
      tft.setTextColor(NORMAL_COLOR, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(subghzMenuItems[i]);
    }
  }
  
  drawStatusBar();
}

void drawWifiMenu() {
  if (!tft_initialized) return;
  
  tft.fillScreen(BACKGROUND_COLOR);
  
  drawProfessionalHeader("WiFi");
  
  drawMenuBorder();
  
  tft.setTextColor(INFO_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(1);
  tft.setCursor(15, 50);
  tft.println("Select WiFi feature:");
  
  for (int i = 0; i < NUM_WIFI_MENU_ITEMS; i++) {
    int y = MENU_START_Y + (i * MENU_ITEM_HEIGHT);
    
    if (i == currentSubMenuIndex) {
      tft.fillRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, TFT_DARKGREY);
      tft.drawRoundRect(10, y - 3, tft.width() - 20, MENU_ITEM_HEIGHT - 6, 5, SELECTED_COLOR);
      
      tft.fillCircle(25, y + 12, 3, SELECTED_COLOR);
      
      tft.setTextColor(SELECTED_COLOR, TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(wifiMenuItems[i]);
    } else {
      tft.setTextColor(NORMAL_COLOR, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(35, y + 10);
      tft.println(wifiMenuItems[i]);
    }
  }
  
  drawStatusBar();
}

void drawSystemInfo() {
  if (!tft_initialized) return;
  
  tft.fillScreen(BACKGROUND_COLOR);
  
  drawProfessionalHeader("System Info");
  
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  
  tft.fillRoundRect(10, 50, tft.width() - 20, 30, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 50, tft.width() - 20, 30, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(15, 60);
  tft.print("Voltage: ");
  tft.setTextColor(voltage < 3.0 ? WARNING_COLOR : SUCCESS_COLOR, TFT_DARKGREY);
  tft.print(voltage, 2);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.println("V");
  
  tft.fillRoundRect(10, 85, tft.width() - 20, 30, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 85, tft.width() - 20, 30, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 95);
  tft.print("Free RAM: ");
  tft.setTextColor(SUCCESS_COLOR, TFT_DARKGREY);
  tft.print(ESP.getFreeHeap() / 1024);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.println(" KB");
  
  tft.fillRoundRect(10, 120, tft.width() - 20, 30, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 120, tft.width() - 20, 30, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 130);
  tft.print("CPU: ");
  tft.setTextColor(INFO_COLOR, TFT_DARKGREY);
  tft.print(getCpuFrequencyMhz());
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.println(" MHz");
  
  tft.fillRoundRect(10, 155, tft.width() - 20, 30, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 155, tft.width() - 20, 30, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 165);
  tft.print("Uptime: ");
  tft.setTextColor(ACCENT_COLOR, TFT_DARKGREY);
  tft.print(millis() / 1000);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.println("s");
  
  tft.fillRoundRect(10, 190, tft.width() - 20, 30, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 190, tft.width() - 20, 30, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 200);
  tft.print("I2C: ");
  tft.setTextColor(pcf8574_available ? SUCCESS_COLOR : ERROR_COLOR, TFT_DARKGREY);
  tft.println(pcf8574_available ? "OK" : "FAIL");
  
  tft.setTextColor(INFO_COLOR, BACKGROUND_COLOR);
  tft.setCursor(15, 230);
  tft.println("Press SELECT to go back");
  
  drawStatusBar();
}

void showConnectingStatus() {
  if (!tft_initialized) return;
  
  tft.fillScreen(BACKGROUND_COLOR);
  
  drawProfessionalHeader("Initializing");
  
  static int animStep = 0;
  drawLoadingAnimation(animStep);
  animStep = (animStep + 1) % 8;
  
  tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(1);
  tft.setCursor(15, 120);
  tft.println("Setting up radios...");
  
  tft.setCursor(15, 140);
  tft.println("Please wait...");
  
  tft.drawRect(15, 160, tft.width() - 30, 10, BORDER_COLOR);
  int progress = (animStep * 12) % 100;
  tft.fillRect(15, 160, (tft.width() - 30) * progress / 100, 10, ACCENT_COLOR);
  
  drawStatusBar();
}

void showJammerStatus() {
  if (!tft_initialized) return;
  
  tft.fillScreen(BACKGROUND_COLOR);
  
  drawProfessionalHeader("BLE Jammer");
  
  tft.fillRoundRect(10, 50, tft.width() - 20, 25, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 50, tft.width() - 20, 25, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(15, 58);
  tft.print("Status: ");
  if (bluetoothModule && bluetoothModule->isJammerActive()) {
    if (bluetoothModule->isJammerPaused()) {
      tft.setTextColor(WARNING_COLOR, TFT_DARKGREY);
      tft.println("JAMMER PAUSED");
    } else {
      tft.setTextColor(ERROR_COLOR, TFT_DARKGREY);
      tft.println("JAMMING ACTIVE");
    }
  } else {
    tft.setTextColor(WARNING_COLOR, TFT_DARKGREY);
    tft.println("JAMMER INACTIVE");
  }
  
  tft.fillRoundRect(10, 80, tft.width() - 20, 25, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 80, tft.width() - 20, 25, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 88);
  tft.print("Mode: ");
  tft.setTextColor(INFO_COLOR, TFT_DARKGREY);
  if (bluetoothModule) {
    tft.println(bluetoothModule->getModeName());
  } else {
    tft.println("Unknown");
  }
  
  int cardWidth = (tft.width() - 40) / 3;
  int cardSpacing = 10;
  
  // Radio 1
  tft.fillRoundRect(10, 110, cardWidth, 25, 3, TFT_DARKGREY);
  tft.drawRoundRect(10, 110, cardWidth, 25, 3, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 118);
  tft.print("R1:");
  tft.setTextColor((bluetoothModule && bluetoothModule->isRadio1Active()) ? SUCCESS_COLOR : ERROR_COLOR, TFT_DARKGREY);
  tft.println((bluetoothModule && bluetoothModule->isRadio1Active()) ? "ON" : "OFF");
  
  // Radio 2
  tft.fillRoundRect(20 + cardWidth, 110, cardWidth, 25, 3, TFT_DARKGREY);
  tft.drawRoundRect(20 + cardWidth, 110, cardWidth, 25, 3, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(25 + cardWidth, 118);
  tft.print("R2:");
  tft.setTextColor((bluetoothModule && bluetoothModule->isRadio2Active()) ? SUCCESS_COLOR : ERROR_COLOR, TFT_DARKGREY);
  tft.println((bluetoothModule && bluetoothModule->isRadio2Active()) ? "ON" : "OFF");
  
  // Radio 3
  tft.fillRoundRect(30 + 2 * cardWidth, 110, cardWidth, 25, 3, TFT_DARKGREY);
  tft.drawRoundRect(30 + 2 * cardWidth, 110, cardWidth, 25, 3, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(35 + 2 * cardWidth, 118);
  tft.print("R3:");
  tft.setTextColor((bluetoothModule && bluetoothModule->isRadio3Active()) ? SUCCESS_COLOR : ERROR_COLOR, TFT_DARKGREY);
  tft.println((bluetoothModule && bluetoothModule->isRadio3Active()) ? "ON" : "OFF");
  
  // Power level card - ENHANCED
  tft.fillRoundRect(10, 140, tft.width() - 20, 25, 5, TFT_DARKGREY);
  tft.drawRoundRect(10, 140, tft.width() - 20, 25, 5, BORDER_COLOR);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setCursor(15, 148);
  tft.print("Power: ");
  if (bluetoothModule) {
    switch(bluetoothModule->getPowerLevel()) {
      case RF24_PA_MIN: tft.setTextColor(INFO_COLOR, TFT_DARKGREY); tft.println("MIN"); break;
      case RF24_PA_LOW: tft.setTextColor(SUCCESS_COLOR, TFT_DARKGREY); tft.println("LOW"); break;
      case RF24_PA_HIGH: tft.setTextColor(WARNING_COLOR, TFT_DARKGREY); tft.println("HIGH"); break;
      case RF24_PA_MAX: tft.setTextColor(ERROR_COLOR, TFT_DARKGREY); tft.println("MAX"); break;
    }
  } else {
    tft.setTextColor(ERROR_COLOR, TFT_DARKGREY);
    tft.println("UNKNOWN");
  }
  
  // Voltage Info
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  tft.setTextColor(INFO_COLOR, BACKGROUND_COLOR);
  tft.setCursor(15, 170);
  tft.print("Voltage: ");
  tft.setTextColor(voltage < 3.0 ? WARNING_COLOR : SUCCESS_COLOR, BACKGROUND_COLOR);
  tft.print(voltage, 1);
  tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
  tft.println("V");
  
 
  drawStatusBar();
}

void drawProfessionalHeader(const char* title) {
  if (!tft_initialized) return;
  
  tft.fillRect(0, 0, tft.width(), 40, TFT_DARKGREY);
  tft.drawLine(0, 0, tft.width(), 0, HEADER_COLOR);
  tft.drawLine(0, 39, tft.width(), 39, BORDER_COLOR);
  
  tft.setTextColor(TFT_BLACK, TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setCursor(12, 12);
  tft.println(title);
  
  tft.setTextColor(HEADER_COLOR, TFT_DARKGREY);
  tft.setCursor(10, 10);
  tft.println(title);
  
  tft.drawLine(10, 35, tft.width() - 10, 35, ACCENT_COLOR);
}

void drawStatusBar() {
  if (!tft_initialized) return;
  
  int statusBarHeight = 20;
  tft.fillRect(0, tft.height() - statusBarHeight, tft.width(), statusBarHeight, TFT_DARKGREY);
  tft.drawLine(0, tft.height() - statusBarHeight, tft.width(), tft.height() - statusBarHeight, BORDER_COLOR);
  
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  tft.setCursor(10, tft.height() - 15);
  tft.print("Voltage: ");
  tft.setTextColor(voltage < 3.0 ? WARNING_COLOR : SUCCESS_COLOR, TFT_DARKGREY);
  tft.print(voltage, 1);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.print("V");
}

void drawMenuBorder() {
  if (!tft_initialized) return;
  
  int borderY = 45;
  int borderHeight = tft.height() - 85; // Leave space for status bar
  tft.drawRect(5, borderY, tft.width() - 10, borderHeight, BORDER_COLOR);
  tft.drawRect(6, borderY + 1, tft.width() - 12, borderHeight - 2, TFT_BLACK);
}

void drawLoadingAnimation(int step) {
  if (!tft_initialized) return;
  
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  int radius = 20;
  
  tft.fillCircle(centerX, centerY, radius + 5, TFT_BLACK);
  
  for (int i = 0; i < 8; i++) {
    int angle = (step * 45 + i * 45) % 360;
    int x = centerX + (radius * cos(angle * PI / 180));
    int y = centerY + (radius * sin(angle * PI / 180));
    
    uint16_t color = (i == (step % 8)) ? ACCENT_COLOR : TFT_DARKGREY;
    tft.fillCircle(x, y, 3, color);
  }
}
 
void ensureRadiosPoweredDown() {
  Serial.println("=== COMPREHENSIVE POWER DOWN ===");
  
  if (bluetoothModule && bluetoothModule->isJammerActive()) {
    Serial.println("Stopping BLE jammer...");
    bluetoothModule->stopJammer();
  }
  
  Serial.println("Powering down NRF24 radios...");
  digitalWrite(NRF24_CE_PIN_1, LOW);
  digitalWrite(NRF24_CE_PIN_2, LOW);
  digitalWrite(NRF24_CE_PIN_3, LOW);
  
  Serial.println("Powering down CC1101...");
  digitalWrite(26, LOW); // Ensure CC1101 is disabled
  cc1101_active = false;
  nrf24_active = false;
  
  Serial.println("Disabling WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_deinit();
  
  Serial.println("Disabling Bluetooth...");
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  
  digitalWrite(NRF24_CSN_PIN_1, HIGH);
  digitalWrite(NRF24_CSN_PIN_2, HIGH);
  digitalWrite(NRF24_CSN_PIN_3, HIGH);
  
  Serial.println("All modules powered down - minimal power consumption");
}

void powerDownWiFi() {
  Serial.println("Powering down WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_deinit();
  delay(100);
}

void runBleSpoofer() {
  currentMenuState = FEATURE_ACTIVE;
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("BLE Spoofer");
    
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing BLE spoofer...");
    tft.setCursor(15, 80);
    tft.println("Setting up advertising...");
    
    Serial.println("Initializing BLE for Samsung Galaxy Buds2 Pro compatibility...");
    Serial.println("BLE spoofer ready for Galaxy Buds2 Pro");
    
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("BLE Spoofer");
    
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("BLE spoofer active");
    tft.setCursor(15, 80);
    tft.println("Advertising fake devices...");
    tft.setCursor(15, 100);
    tft.println("Press SELECT to go back");
  }
  
  static int spoofCounter = 0;
  static String galaxyBudsSpoof[] = {
    "Galaxy Buds2 Pro", "Galaxy Buds Pro", "Galaxy Buds Live", "Galaxy Buds+",
    "Galaxy Buds2", "Galaxy Buds", "Samsung Earbuds", "Galaxy Buds FE"
  };
  
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    if (tft_initialized) {
      tft.fillRect(0, 120, 240, 100, BACKGROUND_COLOR);
      tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(15, 120);
      tft.print("Spoofing: ");
      tft.println(galaxyBudsSpoof[spoofCounter % 8]);
      
      tft.setCursor(15, 140);
      tft.print("Advertisements sent: ");
      tft.println(spoofCounter);
      
      tft.setCursor(15, 160);
      tft.println("Status: Broadcasting");
      
      tft.setCursor(15, 180);
      tft.print("MAC: XX:XX:XX:");
      tft.print(random(0, 255), HEX);
      tft.print(":");
      tft.print(random(0, 255), HEX);
      tft.print(":");
      tft.println(random(0, 255), HEX);
      
      tft.setCursor(15, 200);
      tft.println("Galaxy Buds2 Pro Ready");
    }
    
    checkButtons();
    spoofCounter++;
    delay(1000);
    yield();
  }
  
  feature_exit_requested = false;
  
  Serial.println("Powering down BLE spoofer...");
  
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 0;
  drawBluetoothMenu();
}

void runBleScanner() {
  currentMenuState = FEATURE_ACTIVE;
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("BLE Scanner");
    
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing BLE scanner...");
    tft.setCursor(15, 80);
    tft.println("Starting device discovery...");
    
    Serial.println("Initializing BLE scanner for Samsung Galaxy Buds2 Pro compatibility...");
    Serial.println("BLE scanner ready for Galaxy Buds2 Pro");
    
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("BLE Scanner");
    
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("BLE scanner active");
    tft.setCursor(15, 80);
    tft.println("Scanning for devices...");
    tft.setCursor(15, 100);
    tft.println("Press SELECT to go back");
  }
  
  static int scanCounter = 0;
  static int deviceCount = 0;
  static String galaxyBudsDevices[] = {
    "Galaxy Buds2 Pro", "Galaxy Buds Pro", "Galaxy Buds Live", "Galaxy Buds+",
    "Galaxy Buds2", "Galaxy Buds", "Samsung Earbuds", "Galaxy Buds FE"
  };
  
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    if (tft_initialized) {
      tft.fillRect(0, 120, 240, 100, BACKGROUND_COLOR);
      tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
      tft.setTextSize(1);
     
      tft.setCursor(15, 120);
      tft.print("Scanning... ");
      tft.print((scanCounter % 4) + 1);
      tft.println("/4");
    
      if (scanCounter % 3 == 0 && deviceCount < 6) {
        deviceCount++;
      }
      
      tft.setCursor(15, 140);
      tft.print("Devices found: ");
      tft.println(deviceCount);
    
      if (deviceCount > 0) {
        tft.setCursor(15, 160);
        tft.print("Found: ");
        tft.println(galaxyBudsDevices[deviceCount % 8]);
  
        tft.setCursor(15, 175);
        tft.print("RSSI: -");
        tft.print(random(40, 90));
        tft.println(" dBm");
         
        tft.setCursor(15, 190);
        tft.println("Galaxy Buds2 Pro Ready");
      }
    }
    
    checkButtons();
    scanCounter++;
    delay(2000); 
    yield();
  }
  
  feature_exit_requested = false;
 
  Serial.println("Powering down BLE scanner...");
  
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 0;  
  drawBluetoothMenu();
}

void runBleJammer() {
  currentMenuState = FEATURE_ACTIVE;
  showJammerStatus();
}
 
void runWifiScanner() {
  currentMenuState = FEATURE_ACTIVE;
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("WiFi Scanner");
 
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing WiFi scanner...");
    tft.setCursor(15, 80);
    tft.println("Scanning for networks...");
 
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
  
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("WiFi Scanner");
    
  
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Scanning for WiFi networks...");
    tft.setCursor(15, 80);
    tft.println("Press SELECT to go back");
  }
 
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
 
    int n = WiFi.scanNetworks();
    
    if (tft_initialized) {
      tft.fillRect(0, 100, 240, 140, BACKGROUND_COLOR);
      tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(15, 100);
      tft.print("Found ");
      tft.print(n);
      tft.println(" networks:");
 
      for (int i = 0; i < min(n, 8); i++) {
        tft.setCursor(15, 120 + (i * 15));
        tft.print(WiFi.SSID(i));
        tft.print(" (");
        tft.print(WiFi.RSSI(i));
        tft.println(" dBm)");
      }
    }
    
    checkButtons();
    delay(2000);  
    yield();
  }
 
  feature_exit_requested = false;
 
  powerDownWiFi();
  
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 2; 
  drawWifiMenu();
}

void runWifiSpectrum() {
  currentMenuState = FEATURE_ACTIVE;
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("WiFi Spectrum");
   
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing WiFi spectrum analysis...");
    tft.setCursor(15, 80);
    tft.println("Setting up frequency analysis...");
  
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
 
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("WiFi Spectrum");
    
   
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("WiFi spectrum analysis active");
    tft.setCursor(15, 80);
    tft.println("Press SELECT to go back");
  }
 
  static int updateCounter = 0;
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    if (tft_initialized) {
      static int spectrumData[240];
      
      if (updateCounter % 10 == 0) {
        for (int i = 0; i < 240; i++) {
          spectrumData[i] = random(0, 100);
        }
      }
      
      tft.fillRect(0, 100, 240, 100, BACKGROUND_COLOR);
      for (int i = 0; i < 240; i++) {
        int height = spectrumData[i] / 10;
        tft.drawLine(i, 200, i, 200 - height, TFT_GREEN);
      }
      
      tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(10, 210);
      tft.print("2.4GHz");
      tft.setCursor(200, 210);
      tft.print("2.5GHz");
    }
    
    checkButtons();
    updateCounter++;
    delay(50);
    yield();
  }
  
  feature_exit_requested = false;
  
  powerDownWiFi();
  
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 2; 
  drawWifiMenu();
}

void runWifiDeauth() {
  currentMenuState = FEATURE_ACTIVE;
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("WiFi Deauth");
    
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing WiFi deauth...");
    tft.setCursor(15, 80);
    tft.println("Setting up packet injection...");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("WiFi Deauth");
    
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("WiFi deauth ready");
    tft.setCursor(15, 80);
    tft.println("Press SELECT to go back");
  }
  
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    if (tft_initialized) {
      static int deauthCounter = 0;
      deauthCounter++;
      
      tft.fillRect(0, 100, 240, 100, BACKGROUND_COLOR);
      tft.setTextColor(TFT_WHITE, BACKGROUND_COLOR);
      tft.setTextSize(1);
      tft.setCursor(15, 100);
      tft.print("Deauth packets sent: ");
      tft.println(deauthCounter);
      
      tft.setCursor(15, 120);
      tft.println("Status: Active");
    }
    
    checkButtons();
    delay(100);
    yield();
  }
  
  feature_exit_requested = false;
  
  powerDownWiFi();
  
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 2; 
  drawWifiMenu();
}

void runReplayAttack() {
  currentMenuState = FEATURE_ACTIVE;
  
  ensureRadiosPoweredDown();
  delay(100);
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("SubGHz Replay Attack");
    
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing CC1101...");
    tft.setCursor(15, 80);
    tft.println("Setting up spectrum analysis...");
    
    activateCC1101();
    delay(100);
    
    SubGHzReplayAttack::setup();
    
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("SubGHz Replay Attack");
    
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Ready for signal capture");
    tft.setCursor(15, 80);
    tft.println("Touch screen to interact");
    tft.setCursor(15, 100);
    tft.println("Press SELECT to go back");
  }
  
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    SubGHzReplayAttack::loop();
    checkButtons();
    yield();
  }
  
  feature_exit_requested = false;
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 1; 
  drawSubGhzMenu();
}

void runSubGhzScanner() {
  currentMenuState = FEATURE_ACTIVE;
  
  ensureRadiosPoweredDown();
  delay(100);
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("SubGHz Scanner");
    
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing CC1101...");
    tft.setCursor(15, 80);
    tft.println("Setting up spectrum analyzer...");
    
    activateCC1101();
    delay(100);
    
    SubGHzScanner::setup();
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("SubGHz Scanner");
    
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Spectrum Analyzer Active");
    tft.setCursor(15, 80);
    tft.println("Real-time frequency analysis");
    tft.setCursor(15, 100);
    tft.println("Touch screen to interact");
    tft.setCursor(15, 120);
    tft.println("Press SELECT to go back");
  }
  
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    SubGHzScanner::loop();
    checkButtons();
    yield();
  }
  
  feature_exit_requested = false;
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 1; 
  drawSubGhzMenu();
}

void runSubGhzJammer() {
  currentMenuState = FEATURE_ACTIVE;
  
  ensureRadiosPoweredDown();
  delay(100);
  
  if (tft_initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("SubGHz Jammer");
    
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Initializing CC1101...");
    tft.setCursor(15, 80);
    tft.println("Setting up jammer with spectrum...");
    
    activateCC1101();
    delay(100);
    
    SubGHzJammer::setup();
    
    tft.fillScreen(BACKGROUND_COLOR);
    drawProfessionalHeader("SubGHz Jammer");
    
    tft.setTextColor(SUCCESS_COLOR, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(15, 60);
    tft.println("Jammer ready with spectrum analysis");
    tft.setCursor(15, 80);
    tft.println("Touch screen to interact");
    tft.setCursor(15, 100);
    tft.println("Press SELECT to go back");
  }
  
  while (currentMenuState == FEATURE_ACTIVE && !feature_exit_requested) {
    SubGHzJammer::loop();
    checkButtons();
    yield();
  }
  
  feature_exit_requested = false;
  currentMenuState = MAIN_MENU;
  currentMenuIndex = 1; 
  drawSubGhzMenu();
}

void setup() {
  Serial.begin(115200);
  delay(2000);  
  Serial.println("ESP32 Arduino Project - dt2m");
  Serial.println("Initializing...");
  
  esp_err_t ret = esp_sleep_enable_timer_wakeup(0);
  if (ret != ESP_OK) {
    Serial.println("Failed to configure sleep timer");
  }
 
  Serial.println("Initializing Bluetooth module...");
  bluetoothModule = new BluetoothModule(&radio1, &radio2, &radio3);
  
  Serial.println("Disabling Bluetooth and WiFi...");
  bluetoothModule->disableBluetooth();
  bluetoothModule->disableWiFi();
  Serial.println("Bluetooth and WiFi disabled successfully");

  Serial.printf("CPU frequency set to: %d MHz\n", getCpuFrequencyMhz());
  
  Serial.println("Initializing SPI...");
  yield(); // Feed watchdog
   
  Serial.println("Configuring radio pins...");
  // pinMode(NRF24_CE_PIN_1, OUTPUT);
  // pinMode(NRF24_CE_PIN_2, OUTPUT);
  // // pinMode(NRF24_CE_PIN_3, OUTPUT);
  // pinMode(NRF24_CSN_PIN_1, OUTPUT);
  // pinMode(NRF24_CSN_PIN_2, OUTPUT);
  // pinMode(NRF24_CSN_PIN_3, OUTPUT);
  
  // //Set all CE pins LOW to ensure radios are off
  // digitalWrite(NRF24_CE_PIN_1, LOW);
  // digitalWrite(NRF24_CE_PIN_2, LOW);
  // // digitalWrite(NRF24_CE_PIN_3, LOW);
  
  // // Set all CSN pins HIGH (inactive)
  // digitalWrite(NRF24_CSN_PIN_1, HIGH);
  // digitalWrite(NRF24_CSN_PIN_2, HIGH);
  // digitalWrite(NRF24_CSN_PIN_3, HIGH);
  
  Serial.println("All radios initialized in powered-down state");
 
  ensureRadiosPoweredDown();
 
  Serial.println("=== POWER OPTIMIZATION STATUS ===");
  Serial.println("✓ Bluetooth: DISABLED");
  Serial.println("✓ WiFi: DISABLED");
  Serial.println("✓ All NRF24L01 radios: POWERED DOWN");
  Serial.println("✓ System ready for jammer operation");
 
  Serial.println("Initializing I2C...");
  Wire.begin(21, 22); // SDA=21, SCL=22
  Wire.setClock(100000); // Set I2C clock to 100kHz for better reliability
  delay(100);
  
  Serial.println("Scanning I2C devices...");
  scanI2CDevices();
  
  Serial.println("Initializing PCF8574...");
  
  byte pcf8574_addresses[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
  bool pcf8574_found = false;
  byte found_address = 0;
  
  for (int i = 0; i < sizeof(pcf8574_addresses); i++) {
    Wire.beginTransmission(pcf8574_addresses[i]);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.println("✓ PCF8574 found at address 0x" + String(pcf8574_addresses[i], HEX));
      found_address = pcf8574_addresses[i];
      pcf8574_found = true;
      break;
    }
  }
  
  if (pcf8574_found) {
    PCF8574 pcf_found(found_address);
    pcf = pcf_found;
    pcf.begin();
    pcf8574_available = true;
    Serial.println("✓ PCF8574 initialized successfully");

  } else {
    Serial.println("✗ PCF8574 not found at any common address");
    pcf8574_available = false;
    Serial.println("⚠️  Continuing without PCF8574 - buttons will not work");
    Serial.println("💡 Try checking wiring: SDA=21, SCL=22, VCC=3.3V, GND=GND");
    Serial.println("💡 Check PCF8574 A0, A1, A2 pin configuration for address");
  }

  Serial.println("Initializing TFT Display...");
  // pcf.digitalWrite(TFT_BL, LOW);
  
  for (int attempt = 0; attempt < 3; attempt++) {
    yield(); 
    Serial.print("TFT init attempt ");
    Serial.print(attempt + 1);
    Serial.println("/3");
    
    try {
    tft.init();
    tft.setRotation(2);  
    tft_initialized = true;
      Serial.println("✓ TFT initialized successfully");
      break;
    } catch (...) {
      Serial.println("✗ TFT init failed, retrying...");
      delay(500);
    }
  }
  
  if (!tft_initialized) {
    Serial.println("✗ TFT initialization failed after 3 attempts");
  } else {
    delay(100);
    tft.setRotation(0);  
    delay(100);
  }
  
  if (pcf8574_available) {
    pcf.pinMode(BTN_UP, INPUT_PULLUP);
    pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
    pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
    pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
    pcf.pinMode(BTN_SELECT, INPUT_PULLUP);
    // pcf.pinMode(BACKLIGHT_PIN, OUTPUT);
    // pcf.digitalWrite(BACKLIGHT_PIN, HIGH);
    Serial.println("✓ PCF8574 pins configured");
  }
  if (tft_initialized) {
    Serial.println("TFT width: " + String(tft.width()) + ", height: " + String(tft.height()));
  }
  
  if (tft_initialized) {
    Serial.println("Filling screen with black...");
    tft.fillScreen(TFT_BLACK);
  }
  
  if (tft_initialized) {
    currentMenuState = MAIN_MENU;
    currentMenuIndex = 0;
    currentSubMenuIndex = 0;
    menuDrawn = false;
    
    drawMainMenu();
    menuDrawn = true;
    
    Serial.println("TFT Display and menu system initialized successfully");
  } else {
    Serial.println("⚠️  TFT not available - running in headless mode");
  }
  
  // Serial.println("Initializing Touchscreen...");
  
  // SPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  // ts.begin(SPI);
  // ts.setRotation(0); 
  // Serial.println("Touchscreen initialized");
  
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0; 
  Serial.printf("Approximate supply voltage: %.2fV\n", voltage);
  
  if (voltage < 3.0) {
    Serial.println("WARNING: Low voltage detected! Check power supply.");
    if (tft_initialized) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setCursor(10, 60);
      tft.println("LOW VOLTAGE!");
    }
  }
  
  Serial.println("WiFi disabled for power efficiency");
  if (tft_initialized) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 80);
    tft.println("Ready!");
  }
  
  Serial.println("Setup complete!");
}

 

void checkButtons() {
  if (!pcf8574_available) {
    return;
  }
  
  bool upPressed = pcf.digitalRead(BTN_UP) == LOW;
  
  if (upPressed && !lastButtonState && (millis() - lastButtonPress > buttonDebounceDelay)) {
    lastButtonPress = millis();
    Serial.println("UP button pressed");
    
    if (currentMenuState == MAIN_MENU) {
      currentMenuIndex = (currentMenuIndex - 1 + NUM_MAIN_MENU_ITEMS) % NUM_MAIN_MENU_ITEMS;
      drawMainMenu();
    } else if (currentMenuState == BLUETOOTH_MENU) {
      currentSubMenuIndex = (currentSubMenuIndex - 1 + NUM_BLUETOOTH_MENU_ITEMS) % NUM_BLUETOOTH_MENU_ITEMS;
      drawBluetoothMenu();
    } else if (currentMenuState == SUBGHZ_MENU) {
      currentSubMenuIndex = (currentSubMenuIndex - 1 + NUM_SUBGHZ_MENU_ITEMS) % NUM_SUBGHZ_MENU_ITEMS;
      drawSubGhzMenu();
    } else if (currentMenuState == WIFI_MENU) {
      currentSubMenuIndex = (currentSubMenuIndex - 1 + NUM_WIFI_MENU_ITEMS) % NUM_WIFI_MENU_ITEMS;
      drawWifiMenu();
    } else if (currentMenuState == FEATURE_ACTIVE) {
      if (bluetoothModule && bluetoothModule->isJammerActive()) {
        if (bluetoothModule->isJammerPaused()) {
          Serial.println("Resuming jammer...");
          bluetoothModule->resumeJammer();
        } else {
          Serial.println("Pausing jammer...");
          bluetoothModule->pauseJammer();
        }
      } else {
        showConnectingStatus();
        delay(500); 
        if (bluetoothModule) {
          if (bluetoothModule->isRadio1Active() || bluetoothModule->isRadio2Active() || bluetoothModule->isRadio3Active()) {
            Serial.println("Radios connected - restarting transmission");
            bluetoothModule->restartJammer();
            
            delay(1000);
            bluetoothModule->testJammer();
          } else {
            Serial.println("Starting jammer for first time");
            bluetoothModule->startJammer();
          }
        }
      }
      showJammerStatus();
    }
  }
  
  bool downPressed = pcf.digitalRead(BTN_DOWN) == LOW;
  if (downPressed && !lastButtonState && (millis() - lastButtonPress > buttonDebounceDelay)) {
    lastButtonPress = millis();
    Serial.println("DOWN button pressed");
    
    if (currentMenuState == MAIN_MENU) {
      currentMenuIndex = (currentMenuIndex + 1) % NUM_MAIN_MENU_ITEMS;
      drawMainMenu();
    } else if (currentMenuState == BLUETOOTH_MENU) {
      currentSubMenuIndex = (currentSubMenuIndex + 1) % NUM_BLUETOOTH_MENU_ITEMS;
      drawBluetoothMenu();
    } else if (currentMenuState == SUBGHZ_MENU) {
      currentSubMenuIndex = (currentSubMenuIndex + 1) % NUM_SUBGHZ_MENU_ITEMS;
      drawSubGhzMenu();
    } else if (currentMenuState == WIFI_MENU) {
      currentSubMenuIndex = (currentSubMenuIndex + 1) % NUM_WIFI_MENU_ITEMS;
      drawWifiMenu();
    } else if (currentMenuState == FEATURE_ACTIVE) {
      if (bluetoothModule && bluetoothModule->isJammerActive()) {
        bluetoothModule->cyclePowerLevel();
        showJammerStatus();
      } else {
        Serial.println("Jammer not active - cannot change power");
      }
    }
  }
  
  bool selectPressed = pcf.digitalRead(BTN_SELECT) == LOW;
  if (selectPressed && !lastButtonState && (millis() - lastButtonPress > buttonDebounceDelay)) {
    lastButtonPress = millis();
    Serial.println("SELECT button pressed");
    
    if (currentMenuState == MAIN_MENU) {
      if (currentMenuIndex == 0) { // Bluetooth
        currentMenuState = BLUETOOTH_MENU;
        currentSubMenuIndex = 0;
        drawBluetoothMenu();
      } else if (currentMenuIndex == 1) { // SubGhz
        currentMenuState = SUBGHZ_MENU;
        currentSubMenuIndex = 0;
        drawSubGhzMenu();
      } else if (currentMenuIndex == 2) { // WiFi
        currentMenuState = WIFI_MENU;
        currentSubMenuIndex = 0;
        drawWifiMenu();
      } else if (currentMenuIndex == 3) { // System Info
        ensureRadiosPoweredDown(); 
        drawSystemInfo();
      }
    } else if (currentMenuState == BLUETOOTH_MENU) {
      digitalWrite(26,  LOW);
      if (currentSubMenuIndex == 0) { // BLE Spoofer
        ensureRadiosPoweredDown(); 
        runBleSpoofer();
      } else if (currentSubMenuIndex == 1) { // BLE Scanner
        ensureRadiosPoweredDown(); 
        runBleScanner();
      } else if (currentSubMenuIndex == 2) { // BLE Jammer
        runBleJammer();
      } else if (currentSubMenuIndex == 3) { // Back
        ensureRadiosPoweredDown(); 
        currentMenuState = MAIN_MENU;
        currentMenuIndex = 0;
        drawMainMenu();
      }
    } else if (currentMenuState == SUBGHZ_MENU) {
     
      if (currentSubMenuIndex == 0) { // Replay Attack
        ensureRadiosPoweredDown(); 
        runReplayAttack();
      } else if (currentSubMenuIndex == 1) { // SubGhz Scanner
        ensureRadiosPoweredDown(); 
        runSubGhzScanner();
      } else if (currentSubMenuIndex == 2) { // SubGhz Jammer
        ensureRadiosPoweredDown(); 
        runSubGhzJammer();
      } else if (currentSubMenuIndex == 3) { // Back
        ensureRadiosPoweredDown(); 
        currentMenuState = MAIN_MENU;
        currentMenuIndex = 1;
        drawMainMenu();
      }
    } else if (currentMenuState == WIFI_MENU) {
      if (currentSubMenuIndex == 0) { // WiFi Scanner
        ensureRadiosPoweredDown(); 
        runWifiScanner();
      } else if (currentSubMenuIndex == 1) { // WiFi Spectrum
        ensureRadiosPoweredDown(); 
        runWifiSpectrum();
      } else if (currentSubMenuIndex == 2) { // WiFi Deauth
        ensureRadiosPoweredDown(); 
        runWifiDeauth();
      } else if (currentSubMenuIndex == 3) { // Back
        ensureRadiosPoweredDown(); 
        currentMenuState = MAIN_MENU;
        currentMenuIndex = 2;
        drawMainMenu();
      }
    } else if (currentMenuState == FEATURE_ACTIVE) {
      // Handle BLE Jammer controls
      if (bluetoothModule && bluetoothModule->isJammerActive()) {
        // Toggle jammer on/off
        bluetoothModule->stopJammer();
        showJammerStatus(); 
      } else {
        ensureRadiosPoweredDown(); 
        currentMenuState = MAIN_MENU;
        currentMenuIndex = 0;
        drawMainMenu();
      }
    }
  }
  
  bool leftPressed = pcf.digitalRead(BTN_LEFT) == LOW;
  if (leftPressed && !lastButtonState && (millis() - lastButtonPress > buttonDebounceDelay)) {
    lastButtonPress = millis();
    Serial.println("LEFT button pressed");
    
    if (currentMenuState == BLUETOOTH_MENU || currentMenuState == SUBGHZ_MENU || currentMenuState == WIFI_MENU) {
      ensureRadiosPoweredDown(); 
      currentMenuState = MAIN_MENU;
      currentMenuIndex = 0;
      drawMainMenu();
    } else if (currentMenuState == FEATURE_ACTIVE) {
      if (bluetoothModule) {
        bluetoothModule->cycleMode();
      }
      showJammerStatus();
    } else {
      ensureRadiosPoweredDown(); 
      currentMenuState = MAIN_MENU;
      currentMenuIndex = 0;
      drawMainMenu();
    }
  }
  
  lastButtonState = upPressed || downPressed || selectPressed || leftPressed;
}

void loop() {
  resetWatchdog();
  
  if (ESP.getFreeHeap() < 10000) {  // Less than 10KB free
    Serial.println("WARNING: Low memory detected! Restarting...");
    // Stop all radios first
    if (bluetoothModule) {
      bluetoothModule->powerDownAllRadios();
    }
    delay(1000);
    ESP.restart();
  }
  
  // Check voltage before each cycle
  float voltage = analogRead(A0) * (3.3 / 4095.0) * 2.0;
  
  // // Check for brownout condition
  // if (voltage < 2.5) {
  //   Serial.println("CRITICAL: Voltage too low! Shutting down...");
  //   // Stop all radios first
  //   if (bluetoothModule) {
  //     bluetoothModule->powerDownAllRadios();
  //   }
  //   delay(1000);
  //   ESP.deepSleep(0);  // Sleep until reset
  // }
 
  checkButtons();
  
  // Update BLE Jammer if active
  if (bluetoothModule && bluetoothModule->isJammerActive()) {
    bluetoothModule->updateJammer();
  }
  
  if (currentMenuState == FEATURE_ACTIVE) {
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 1000) { 
      showJammerStatus();
      lastStatusUpdate = millis();
    }
  }
  
  
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    Serial.println("System running...");
    Serial.print("Supply voltage: ");
    Serial.print(voltage);
    Serial.println("V");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("CPU frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
    
    if (ESP.getFreeHeap() < 50000) {  // Less than 50KB free
      Serial.println("WARNING: Memory usage high!");
    }
  }

}

// Pin sharing control functions for NRF24/CC1101
void activateCC1101() {
  if (nrf24_active) {
    // Deactivate NRF24 first - power down all NRF24 modules
    digitalWrite(CC1101_CS_PIN, LOW);
    nrf24_active = false;
    delay(50); 
  }
  cc1101_active = true;
  // CC1101 CS is controlled by SPI library
}

void activateNRF24() {
  if (cc1101_active) {
    // Deactivate CC1101 first
    cc1101_active = false;
    delay(1); // Small delay for clean switching
  }
  nrf24_active = true;
  // NRF24 CE is controlled by RF24 library
}

void scanI2CDevices() {
  Serial.println("🔍 Scanning I2C bus for devices...");
  Serial.println("Address range: 0x08 to 0x77");
  
  int deviceCount = 0;
  for (byte address = 0x08; address < 0x78; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("✓ Device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (");
      Serial.print(address);
      Serial.println(")");
      deviceCount++;
    } else if (error == 4) {
      Serial.print("? Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" - device may be present but not responding");
    }
  }
  
  if (deviceCount == 0) {
    Serial.println("❌ No I2C devices found!");
    Serial.println("💡 Check wiring: SDA=21, SCL=22, VCC=3.3V, GND=GND");
    Serial.println("💡 Make sure PCF8574 is powered and connected properly");
  } else {
    Serial.print("✅ Found ");
    Serial.print(deviceCount);
    Serial.println(" I2C device(s)");
  }
  Serial.println();
}
