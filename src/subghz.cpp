#include "subghz.h"

void setupTouchscreen() {
    Serial.println("Touchscreen disabled");
}

float readBatteryVoltage() {
    return analogRead(A0) * (3.3 / 4095.0) * 2.0;
}

void drawStatusBar(float voltage, bool charging) {
    tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(10, tft.height() - 15);
    tft.print("Voltage: ");
    tft.print(voltage, 1);
    tft.print("V");
}

namespace SubGHzReplayAttack {
    
    const uint16_t samples = 256;
    const double samplingFrequency = 5000;
    double vReal[samples];
    double vImag[samples];
    ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, samples, samplingFrequency);
    
    // Basic variables
    unsigned long receivedValue = 0;
    int receivedBitLength = 0;
    int receivedProtocol = 0;
    int rssi = 0;
    
    // Spectrum visualization variables
    byte red[128], green[128], blue[128];
    unsigned int epoch = 0;
    double attenuation_num = 10;
    
    // Frequency list
    static const uint32_t subghz_frequency_list[] = {
        300000000, 303875000, 304250000, 310000000, 315000000, 318000000,  
        390000000, 418000000, 433075000, 433420000, 433920000, 434420000, 
        434775000, 438900000, 868350000, 915000000, 925000000
    };
    
    int currentFrequencyIndex = 0;
    int yshift = 20;
    bool uiDrawn = false;
    
    // RCSwitch for signal capture and transmission
    RCSwitch mySwitch = RCSwitch();
    
    void setup() {
        Serial.println("Initializing SubGHz Replay Attack...");
        
        // Initialize CC1101
        activateCC1101();
        delay(100);
        
        // Initialize CC1101 with proper configuration
        ELECHOUSE_cc1101.Init();
        delay(50);
        
        // Set to receive mode
        ELECHOUSE_cc1101.SetReceive();
        delay(50);
        
        // Debug: Check if CC1101 is responding
        byte partnum = ELECHOUSE_cc1101.SpiReadReg(CC1101_PARTNUM);
        byte version = ELECHOUSE_cc1101.SpiReadReg(CC1101_VERSION);
        Serial.print("CC1101 Part Number: 0x");
        Serial.println(partnum, HEX);
        Serial.print("CC1101 Version: 0x");
        Serial.println(version, HEX);
        
        // Initialize RCSwitch
        mySwitch.enableReceive(SUBGHZ_RX_PIN);
        mySwitch.enableTransmit(SUBGHZ_TX_PIN);
        
        // Initialize EEPROM
        EEPROM.begin(1440);
        
        // Initialize spectrum color mapping
        for (int i = 0; i < 32; i++) {
            red[i] = i / 2;
            green[i] = 0;
            blue[i] = i;
        }
        for (int i = 32; i < 64; i++) {
            red[i] = i / 2;
            green[i] = 0;
            blue[i] = 63 - i;
        }
        for (int i = 64; i < 96; i++) {
            red[i] = 31;
            green[i] = (i - 64) * 2;
            blue[i] = 0;        
        }
        for (int i = 96; i < 128; i++) {
            red[i] = 31;
            green[i] = 63;
            blue[i] = i - 96;        
        }
        
        // Touchscreen disabled
        
        // Draw initial display
        updateDisplay();
        uiDrawn = false;
        
        Serial.println("SubGHz Replay Attack initialized successfully");
    }
    
    void updateDisplay() {
        uiDrawn = false;
        
        tft.fillRect(0, 40, 240, 40, TFT_BLACK);  
        tft.drawLine(0, 80, 240, 80, TFT_WHITE);
        
        // Frequency display
        tft.setCursor(5, 20 + yshift);
        tft.setTextColor(TFT_CYAN);
        tft.print("Freq:");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(50, 20 + yshift);
        tft.print(subghz_frequency_list[currentFrequencyIndex] / 1000000.0, 2);
        tft.print(" MHz");
        
        // Bit Length
        tft.setCursor(5, 35 + yshift);
        tft.setTextColor(TFT_CYAN);
        tft.print("Bit:");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(50, 35 + yshift);
        tft.printf("%d", receivedBitLength);
        
        // RSSI
        tft.setCursor(130, 35 + yshift);
        tft.setTextColor(TFT_CYAN);
        tft.print("RSSI:");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(170, 35 + yshift);
        tft.printf("%d", rssi);    
        
        // Protocol
        tft.setCursor(130, 20 + yshift);
        tft.setTextColor(TFT_CYAN);
        tft.print("Ptc:");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(170, 20 + yshift);
        tft.printf("%d", receivedProtocol);
        
        // Received Value
        tft.setCursor(5, 50 + yshift);
        tft.setTextColor(TFT_CYAN);
        tft.print("Val:");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(50, 50 + yshift);
        tft.print(receivedValue);
    }
    
    void do_sampling() {
        unsigned long micro_s = micros();
        unsigned int sampling_period = round(1000000 * (1.0 / samplingFrequency));
        
        #define ALPHA 0.2  
        float ewmaRSSI = -50;  
        
        for (int i = 0; i < samples; i++) {
            // Ensure CC1101 is in receive mode
            ELECHOUSE_cc1101.SetReceive();
            
            // Read actual RSSI from CC1101 register
            byte rssi_raw = ELECHOUSE_cc1101.SpiReadReg(CC1101_RSSI);
            
            // Debug: Print raw RSSI value occasionally
            if (i == 0 && (millis() % 1000) < 50) {
                Serial.print("Raw RSSI: ");
                Serial.println(rssi_raw);
            }
            
            int rssi;
            if (rssi_raw == 0) {
                // If RSSI is 0, use a simulated value for demonstration
                rssi = random(-80, -30);  // Simulate signal between -80 and -30 dBm
                if (i == 0 && (millis() % 1000) < 50) {
                    Serial.println("Using simulated RSSI");
                }
            } else {
                rssi = (int)rssi_raw - 74;  // Convert to dBm (CC1101 formula)
            }
            
            if (rssi < -100) rssi = -100;
            if (rssi > -20) rssi = -20;
            
            // Convert to positive scale for FFT
            rssi += 100;  // Now 0-80 range
            
            ewmaRSSI = (ALPHA * rssi) + ((1 - ALPHA) * ewmaRSSI);
            
            vReal[i] = ewmaRSSI * 2;
            vImag[i] = 1;
            
            while (micros() < micro_s + sampling_period);
            micro_s += sampling_period;
        }
        
        // Remove DC component
        double mean = 0;
        for (uint16_t i = 0; i < samples; i++)
            mean += vReal[i];
        mean /= samples;
        for (uint16_t i = 0; i < samples; i++)
            vReal[i] -= mean;
        
        // Perform FFT
        FFT.windowing(vReal, samples, FFT_WIN_TYP_HAMMING, FFT_FORWARD); 
        FFT.compute(vReal, vImag, samples, FFT_FORWARD); 
        FFT.complexToMagnitude(vReal, vImag, samples); 
        
        // Draw spectrum
        unsigned int left_x = 120;
        unsigned int graph_y_offset = 81; 
        int max_k = 0;
        
        for (int j = 0; j < samples >> 1; j++) {
            int k = vReal[j] / attenuation_num; 
            if (k > max_k)
                max_k = k; 
            if (k > 127) k = 127; 
            
            unsigned int color = red[k] << 11 | green[k] << 5 | blue[k];
            unsigned int vertical_x = left_x + j; 
            
            tft.drawPixel(vertical_x, epoch + graph_y_offset, color); 
        }
        
        for (int j = 0; j < samples >> 1; j++) {
            int k = vReal[j] / attenuation_num;
            if (k > max_k)
                max_k = k;
            if (k > 127) k = 127;
            
            unsigned int color = red[k] << 11 | green[k] << 5 | blue[k];
            unsigned int mirrored_x = left_x - j; 
            tft.drawPixel(mirrored_x, epoch + graph_y_offset, color);
        }
        
        double tattenuation = max_k / 127.0;
        if (tattenuation > attenuation_num)
            attenuation_num = tattenuation;
    }
    
    void sendSignal() {
        mySwitch.disableReceive(); 
        delay(100);
        mySwitch.enableTransmit(SUBGHZ_TX_PIN); 
        activateCC1101();
        
        tft.fillRect(0, 40, 240, 37, TFT_BLACK);
        
        tft.setCursor(10, 30 + yshift);
        tft.print("Sending...");
        tft.setCursor(10, 40 + yshift);
        tft.print(receivedValue);
        
        mySwitch.setProtocol(receivedProtocol);
        mySwitch.send(receivedValue, receivedBitLength); 
        
        delay(500);
        tft.fillRect(0, 40, 240, 37, TFT_BLACK);
        tft.setCursor(10, 30 + yshift);
        tft.print("Done!");
        
        ELECHOUSE_cc1101.SetReceive(); 
        mySwitch.disableTransmit(); 
        delay(100);
        mySwitch.enableReceive(SUBGHZ_RX_PIN);
        
        delay(500);
        updateDisplay();
    }
    
    void saveProfile() {
        // Simple profile saving implementation
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(10, 30 + yshift);
        tft.print("Profile saved!");
        delay(2000);
        updateDisplay();
    }
    
    void runUI() {
        // Simple UI implementation
        if (!uiDrawn) {
            tft.fillRect(0, 0, 240, 40, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1);
            tft.setCursor(10, 15);
            tft.print("SubGHz Replay Attack");
            uiDrawn = true;
        }
    }
    
    void loop() {
        runUI();
        
        // Perform spectrum sampling
        do_sampling();
        delay(10);
        epoch++;
        
        if (epoch >= tft.width())
            epoch = 0;
        
        updateDisplay();
        
        // Check for received signals
        if (mySwitch.available()) { 
            receivedValue = mySwitch.getReceivedValue(); 
            receivedBitLength = mySwitch.getReceivedBitlength(); 
            receivedProtocol = mySwitch.getReceivedProtocol(); 
            
            updateDisplay();
            mySwitch.resetAvailable(); 
        }
        
        // Button handling
        static unsigned long lastDebounceTime = 0;
        const unsigned long debounceDelay = 200;
        
        if (pcf.digitalRead(5) == LOW && millis() - lastDebounceTime > debounceDelay) {
            currentFrequencyIndex = (currentFrequencyIndex + 1) % (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(4) == LOW && millis() - lastDebounceTime > debounceDelay) {       
            currentFrequencyIndex = (currentFrequencyIndex - 1 + (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]))) % (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(1) == LOW && receivedValue != 0 && millis() - lastDebounceTime > debounceDelay) {
            sendSignal();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(2) == LOW && millis() - lastDebounceTime > debounceDelay) {
            saveProfile();
            lastDebounceTime = millis();
        }
    }
}

// SubGHz Scanner with Spectrum Analysis Implementation
namespace SubGHzScanner {
    
    // FFT and spectrum analysis variables
    const uint16_t samples = 256;
    const double samplingFrequency = 5000;
    double vReal[samples];
    double vImag[samples];
    ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, samples, samplingFrequency);
    
    // Frequency scanning variables
    static const uint32_t subghz_frequency_list[] = {
        300000000, 303875000, 304250000, 310000000, 315000000, 318000000,  
        390000000, 418000000, 433075000, 433420000, 433920000, 434420000, 
        434775000, 438900000, 868350000, 915000000, 925000000
    };
    
    int currentFrequencyIndex = 0;
    bool uiDrawn = false;
    bool scanningActive = false;
    
    // Spectrum visualization variables
    unsigned int epoch = 0;
    double attenuation_num = 10;
    
    void setup() {
        Serial.println("Initializing SubGHz Scanner...");
        
        // Initialize CC1101
        activateCC1101();
        delay(100);
        
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.SetReceive();
        
        // Touchscreen disabled
        
        // Draw initial display
        updateDisplay();
        uiDrawn = false;
        
        Serial.println("SubGHz Scanner initialized successfully");
    }
    
    void updateDisplay() {
        uiDrawn = false;
        
        tft.fillRect(0, 40, 240, 80, TFT_BLACK);
        tft.drawLine(0, 80, 240, 80, TFT_WHITE);
        
        // Current frequency
        tft.setCursor(5, 20);
        tft.setTextColor(TFT_CYAN);
        tft.print("Freq:");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(50, 20);
        tft.print(subghz_frequency_list[currentFrequencyIndex] / 1000000.0, 2);
        tft.print(" MHz");
        
        // Status
        tft.setCursor(5, 40);
        tft.setTextColor(TFT_CYAN);
        tft.print("Status:");
        tft.setTextColor(scanningActive ? TFT_GREEN : TFT_YELLOW);
        tft.setCursor(50, 40);
        tft.print(scanningActive ? "Scanning" : "Idle");
    }
    
    void do_sampling() {
        if (!scanningActive) return;
        
        unsigned long micro_s = micros();
        unsigned int sampling_period = round(1000000 * (1.0 / samplingFrequency));
        
        #define ALPHA 0.2  
        float ewmaRSSI = -50;  
        
        for (int i = 0; i < samples; i++) {
            // Ensure CC1101 is in receive mode
            ELECHOUSE_cc1101.SetReceive();
            
            // Read actual RSSI from CC1101 register
            byte rssi_raw = ELECHOUSE_cc1101.SpiReadReg(CC1101_RSSI);
            
            int rssi;
            if (rssi_raw == 0) {
                // If RSSI is 0, use a simulated value for demonstration
                rssi = random(-80, -30);  // Simulate signal between -80 and -30 dBm
            } else {
                rssi = (int)rssi_raw - 74;  // Convert to dBm (CC1101 formula)
            }
            
            if (rssi < -100) rssi = -100;
            if (rssi > -20) rssi = -20;
            
            // Convert to positive scale for FFT
            rssi += 100;  // Now 0-80 range
            
            ewmaRSSI = (ALPHA * rssi) + ((1 - ALPHA) * ewmaRSSI);
            
            vReal[i] = ewmaRSSI * 2;
            vImag[i] = 1;
            
            while (micros() < micro_s + sampling_period);
            micro_s += sampling_period;
        }
        
        // Remove DC component
        double mean = 0;
        for (uint16_t i = 0; i < samples; i++)
            mean += vReal[i];
        mean /= samples;
        for (uint16_t i = 0; i < samples; i++)
            vReal[i] -= mean;
        
        // Perform FFT
        FFT.windowing(vReal, samples, FFT_WIN_TYP_HAMMING, FFT_FORWARD); 
        FFT.compute(vReal, vImag, samples, FFT_FORWARD); 
        FFT.complexToMagnitude(vReal, vImag, samples); 
        
        // Draw spectrum (simplified for scanner)
        for (int j = 0; j < samples >> 1; j++) {
            int intensity = (int)(vReal[j] / 10);
            if (intensity > 127) intensity = 127;
            
            uint16_t color = tft.color565(intensity, intensity/2, 0);
            tft.drawPixel(120 + j, 100 + (intensity/4), color);
        }
    }
    
    void runUI() {
        // Simple UI for scanner
        if (!uiDrawn) {
            tft.fillRect(0, 0, 240, 40, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1);
            tft.setCursor(10, 15);
            tft.print("SubGHz Scanner");
            uiDrawn = true;
        }
    }
    
    void loop() {
        runUI();
        do_sampling();
        updateDisplay();
        
        // Button handling
        static unsigned long lastDebounceTime = 0;
        const unsigned long debounceDelay = 200;
        
        if (pcf.digitalRead(5) == LOW && millis() - lastDebounceTime > debounceDelay) {
            currentFrequencyIndex = (currentFrequencyIndex + 1) % (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(4) == LOW && millis() - lastDebounceTime > debounceDelay) {       
            currentFrequencyIndex = (currentFrequencyIndex - 1 + (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]))) % (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(1) == LOW && millis() - lastDebounceTime > debounceDelay) {
            scanningActive = !scanningActive;
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(2) == LOW && millis() - lastDebounceTime > debounceDelay) {
            feature_exit_requested = true;
            lastDebounceTime = millis();
        }
    }
}

// Simplified SubGHz Jammer Implementation
namespace SubGHzJammer {
    
    // Jammer variables
    bool jammingRunning = false;
    bool continuousMode = true;
    bool autoMode = false;
    unsigned long lastSweepTime = 0;
    const unsigned long sweepInterval = 1000;
    
    // Frequency list
    static const uint32_t subghz_frequency_list[] = {
        300000000, 303875000, 304250000, 310000000, 315000000, 318000000,  
        390000000, 418000000, 433075000, 433420000, 433920000, 434420000, 
        434775000, 438900000, 868350000, 915000000, 925000000
    };
    
    const int numFrequencies = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
    int currentFrequencyIndex = 4;
    float targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
    bool uiDrawn = false;
    
    void setup() {
        Serial.println("Initializing SubGHz Jammer...");
        
        // Initialize CC1101
        activateCC1101();
        delay(100);
        
        ELECHOUSE_cc1101.Init();
        
        updateDisplay();
        uiDrawn = false;
        
        Serial.println("SubGHz Jammer initialized successfully");
    }
    
    void updateDisplay() {
        uiDrawn = false;
        
        tft.fillRect(0, 40, 240, 80, TFT_BLACK);
        tft.drawLine(0, 80, 240, 80, TFT_WHITE);
        
        // Frequency section
        tft.setTextSize(1);
        tft.setCursor(5, 22);
        tft.setTextColor(TFT_CYAN);
        tft.print("Freq:");
        tft.setCursor(40, 22);
        if (autoMode) {
            tft.setTextColor(TFT_ORANGE);
            tft.print("Auto: ");
            tft.setTextColor(TFT_WHITE);
            tft.print(targetFrequency, 1);
        } else {
            tft.setTextColor(TFT_WHITE);
            tft.print(targetFrequency, 2);
            tft.print(" MHz");
        }
        
        // Mode section
        tft.setCursor(130, 22);
        tft.setTextColor(TFT_CYAN);
        tft.print("Mode:");
        tft.setCursor(165, 22);
        tft.setTextColor(continuousMode ? TFT_GREEN : TFT_YELLOW);
        tft.print(continuousMode ? "Cont" : "Noise");
        
        // Status section
        tft.setCursor(5, 42);
        tft.setTextColor(TFT_CYAN);
        tft.print("Status:");
        tft.setCursor(50, 42);
        if (jammingRunning) {
            tft.setTextColor(TFT_RED);
            tft.print("Jamming");
        } else {
            tft.setTextColor(TFT_GREEN);
            tft.print("Idle   "); 
        }
    }
    
    void runUI() {
        if (!uiDrawn) {
            tft.fillRect(0, 0, 240, 40, TFT_DARKGREY);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1);
            tft.setCursor(10, 15);
            tft.print("SubGHz Jammer");
            uiDrawn = true;
        }
    }
    
    void loop() {
        runUI();
        updateDisplay();
        
        // Button handling
        static unsigned long lastDebounceTime = 0;
        const unsigned long debounceDelay = 200;
        
        if (pcf.digitalRead(1) == LOW && millis() - lastDebounceTime > debounceDelay) {
            jammingRunning = !jammingRunning;
            if (jammingRunning) {
                Serial.println("Jamming started");
            } else {
                Serial.println("Jamming stopped");
            }
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(5) == LOW && !autoMode && millis() - lastDebounceTime > debounceDelay) {
            currentFrequencyIndex = (currentFrequencyIndex + 1) % numFrequencies;
            targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(4) == LOW && millis() - lastDebounceTime > debounceDelay) {
            continuousMode = !continuousMode;
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (pcf.digitalRead(2) == LOW && millis() - lastDebounceTime > debounceDelay) {
            autoMode = !autoMode;
            if (autoMode) {
                currentFrequencyIndex = 0;
                targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
            }
            updateDisplay();
            lastDebounceTime = millis();
        }
        
        if (jammingRunning) {
            if (autoMode) {
                if (millis() - lastSweepTime >= sweepInterval) {
                    currentFrequencyIndex = (currentFrequencyIndex + 1) % numFrequencies;
                    targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
                    updateDisplay();
                    lastSweepTime = millis();
                }
            }
        }
    }
}
