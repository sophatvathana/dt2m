# DT2M (Don't T to Me) - ESP32 Multi-Purpose RF Development Platform

## 🎓 Educational Purpose

This project is designed for **educational and research purposes only**. It serves as a comprehensive learning platform for understanding:

- **RF Communication Protocols**: Bluetooth, BLE, sub-GHz, and 2.4GHz radio technologies
- **ESP32 Development**: Advanced microcontroller programming and hardware interfacing
- **Signal Processing**: FFT analysis, signal modulation, and digital signal processing
- **Embedded Systems**: Power management, interrupt handling, and real-time programming
- **RF Security Research**: Understanding wireless communication vulnerabilities and countermeasures

## ⚠️ Important Notice

**This project is intended solely for educational purposes and authorized security research. Users are responsible for complying with all applicable laws and regulations in their jurisdiction. The developers do not endorse or encourage any illegal use of this technology.**

## 🔧 Hardware Overview

### Core Components
- **ESP32 DevKit**: Main microcontroller with WiFi and Bluetooth capabilities
- **TFT Touch Display**: 240x320 ILI9341 touchscreen for user interface
- **PCF8574 I2C Expander**: Button interface and GPIO expansion
- **CC1101 Module**: Sub-GHz transceiver for 315MHz, 433MHz, 868MHz, 915MHz
- **nRF24L01 Modules**: 2.4GHz transceivers for Bluetooth and WiFi analysis
- **Touch Controller**: XPT2046 resistive touch interface

### Pin Configuration
```
TFT Display:
- MOSI: GPIO 13
- SCLK: GPIO 14
- CS: GPIO 15
- DC: GPIO 2
- Touch CS: GPIO 33

I2C (PCF8574):
- SDA: GPIO 21
- SCL: GPIO 22

nRF24L01 Modules (3x):
Radio 1:
- CE: GPIO 4
- CSN: GPIO 5
- MOSI: GPIO 23
- MISO: GPIO 19
- SCK: GPIO 18

Radio 2:
- CE: GPIO 16
- CSN: GPIO 27
- MOSI: GPIO 23
- MISO: GPIO 19
- SCK: GPIO 18

Radio 3:
- CE: GPIO 7
- CSN: GPIO 17
- MOSI: GPIO 23
- MISO: GPIO 19
- SCK: GPIO 18

Sub-GHz (CC1101):
- SCK: GPIO 18
- MISO: GPIO 19
- MOSI: GPIO 23
- CS: GPIO 26
- RX: GPIO 16
- TX: GPIO 27
```

## 📚 Educational Features

### 1. RF Spectrum Analysis
- **FFT-based spectrum analysis** for understanding frequency domain
- **Real-time signal visualization** on TFT display
- **Multi-band scanning** across different frequency ranges

### 2. Bluetooth/BLE Learning
- **BLE device discovery** and analysis
- **Bluetooth protocol understanding**
- **Signal strength measurement**
- **Connection parameter analysis**

### 3. Sub-GHz Communication
- **433MHz/315MHz/868MHz/915MHz** band exploration
- **ASK/OOK modulation** learning
- **Signal decoding** and analysis
- **Remote control protocol** understanding

### 4. Embedded Systems Concepts
- **Power management** and sleep modes
- **Interrupt-driven programming**
- **Memory management** and optimization
- **Real-time task scheduling**

## 🛠️ Software Architecture

### Core Modules
- **`main.cpp`**: Main application loop and system initialization
- **`bluetooth.cpp/h`**: Bluetooth and BLE functionality
- **`subghz.cpp/h`**: Sub-GHz radio operations
- **`ELECHOUSE_CC1101.cpp/h`**: CC1101 transceiver driver

### Key Libraries
- **TFT_eSPI**: Display and touch interface
- **PCF8574**: I2C button interface
- **RF24**: nRF24L01 radio communication
- **RCSwitch**: Remote control signal handling
- **ArduinoFFT**: Fast Fourier Transform analysis

## 🚀 Getting Started

### Prerequisites
- **PlatformIO IDE** or **Arduino IDE**
- **ESP32 development board**
- **Required hardware components** (see Hardware Overview)

### Installation
1. Clone the repository:
```bash
git clone https://github.com/sophatvathana/dt2m.git
cd dt2m
```

2. Open in PlatformIO or Arduino IDE

3. Install dependencies (automatically handled by PlatformIO):
```ini
lib_deps = 
    bodmer/TFT_eSPI@^2.5.43
    xreef/PCF8574 library@^2.3.7
    nrf24/RF24@^1.5.0
    nrf24/RF24Network@^2.0.5
    sui77/rc-switch@^2.6.4
    kosme/arduinoFFT@^2.0.4
    paulstoffregen/XPT2046_Touchscreen@0.0.0-alpha+sha.26b691b2c8
```

4. Configure your hardware pins in `platformio.ini`

5. Upload to ESP32

## 📋 Features

- ✅ **Multi-band RF Analysis** (315MHz - 2.4GHz)
- ✅ **Bluetooth/BLE Scanning**
- ✅ **Touch Interface** with intuitive navigation
- ✅ **Real-time Spectrum Analysis**
- ✅ **Signal Recording and Playback**
- ✅ **Power Management**
- ✅ **Modular Software Architecture**

## 🛡️ Legal and Ethical Considerations

- **Educational Use Only**: This project is designed for learning and research
- **Compliance Required**: Users must follow local RF regulations
- **No Malicious Use**: Do not use for unauthorized interference or attacks
- **Responsible Disclosure**: Report security findings through proper channels

## 🤝 Contributing

Contributions are welcome for:
- **Educational content** and documentation
- **Code improvements** and optimizations
- **Hardware compatibility** additions
- **Safety features** and safeguards

## 📄 License

This project is provided for educational purposes. Please ensure compliance with local laws and regulations regarding RF equipment and testing.

## 🔗 Resources

- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [RF Regulations Guide](https://www.fcc.gov/engineering-technology/electromagnetic-compatibility-division/radio-frequency-safety/faq/rf-safety)
- [Bluetooth Specification](https://www.bluetooth.com/specifications/)
- [Sub-GHz ISM Bands](https://en.wikipedia.org/wiki/ISM_radio_band)

---

**Remember: Use this platform responsibly and only for legitimate educational and research purposes.**
