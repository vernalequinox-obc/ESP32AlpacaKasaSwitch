# ESP32 Alpaca Kasa Switch

An ESP32-based ASCOM Alpaca driver that provides seamless integration between TP-Link Kasa smart switches and astronomical software like NINA (Nighttime Imaging 'N' Astronomy).
  
  Jessie Gentry   Devloped by myself for use with all my current Kasa Smart Plugs. Modified to handle child devices. Adaapted to
                  work with Alpaca. Able to scan through setup page all Kasa devices on the network. Stores enabled devices in
                  persistent storage.  Thanks to the following for their contributions to this project:
  KRIS JEARAKUL   The code ESP32AlpacaKasaSwitch for the Kasa smart switch is based on the  originally developed by Kris Jearakul.
                  This version has been adapted to fit into the ESP32AlpacaDevices framework by BigPet.

## Overview

This project transforms your ESP32 into an ASCOM Alpaca server that can discover, configure, and control TP-Link Kasa smart switches on your network. It's perfect for observatory automation, allowing you to control equipment power remotely through standard ASCOM-compatible software.

### Key Features

- 🔍 **Automatic Discovery**: Finds all Kasa devices on your network
- ⚙️ **Selective Configuration**: Choose which switches to expose to ASCOM clients  
- 🌐 **Web Interface**: Easy configuration through built-in web server
- 💾 **Persistent Settings**: Switch selections survive power cycles
- 🚀 **ASCOM Compliant**: Full Alpaca Switch v3 interface support
- 📱 **NINA Integration**: Optimized for NINA sequencing with indexed switch names

## Hardware Requirements

### ESP32 Development Board
- **Recommended**: ESP32 DevKit v1, NodeMCU-32S, or similar
- **Memory**: Minimum 4MB flash recommended
- **Connectivity**: WiFi capability (built into ESP32)

### Supported Kasa Devices
- **Single Outlets**: HS100, HS103, HS105, HS110, etc.
- **Power Strips**: HS300, KP303, KP400 (multi-outlet support)
- **Smart Plugs**: EP10, EP25, KP125, etc.
- **Wall Switches**: HS200, HS210, HS220 series

## Software Requirements

- **Development Environment**: PlatformIO or Arduino IDE
- **ASCOM Client**: NINA, SGP, MaxIm DL, or any Alpaca-compatible software
- **Web Browser**: For configuration interface

## Credits and Libraries

This project is built on the excellent **ESP32AlpacaDevices** library:

- **Library**: ESP32AlpacaDevices
- **Author**: Peter N (@BigPet)
- **Source**: Based on ASCOM Alpaca specification
- **License**: Open source (see library documentation)

Additional libraries used:
- **SLog**: Logging library for ESP32
- **ArduinoJson**: JSON parsing and generation
- **ESP32 Preferences**: Persistent storage
- **AsyncWebServer**: Web interface handling

## Installation

### 1. Hardware Setup
1. Connect your ESP32 to your computer via USB
2. Ensure your ESP32 and Kasa devices are on the same WiFi network

### 2. Software Setup

#### Using PlatformIO (Recommended)
```bash
# Clone the repository
git clone https://github.com/vernalequinox-obc/ESP32AlpacaKasaSwitch.git
cd ESP32AlpacaKasaSwitch

# Open in PlatformIO
code .  # or open with your preferred editor

# Configure WiFi credentials in src/Config.h
# Build and upload
pio run --target upload
```

#### Using Arduino IDE
1. Download the project as ZIP and extract
2. Install required libraries through Library Manager
3. Configure WiFi credentials in `src/Config.h`
4. Select your ESP32 board and upload

### 3. WiFi Configuration
Edit `src/Config.h` with your network credentials:
```cpp
const char* ssid = "YourWiFiNetwork";
const char* password = "YourWiFiPassword";
```

## How to Use

### Initial Setup

1. **Upload Firmware**: Flash the ESP32 with the project code
2. **Power On**: Connect ESP32 to power and wait for WiFi connection
3. **Find IP Address**: Check your router or serial monitor for ESP32's IP address
4. **Access Web Interface**: Open `http://ESP32_IP_ADDRESS/setup/v1/switch/0/setup` in your browser

### Device Discovery and Configuration

1. **Discover Devices**:
   - Click "Discover Kasa Devices" button in web interface
   - Wait for network scan to complete (5-10 seconds)
   - All Kasa devices on your network will be listed

2. **Select Switches**:
   - Toggle switches ON/OFF to enable/disable them for ASCOM use
   - Enabled switches will be available to NINA and other ASCOM clients
   - Each switch shows: Index number, Name, Model, IP address

3. **Save Configuration**:
   - Click "Save" to store your selections
   - Settings persist across power cycles
   - Use "Re-check Saved" to verify saved devices are still reachable

### ASCOM Client Integration

#### NINA Integration
1. **Add Switch Device**:
   - Go to Equipment → Switch
   - Add new device: "ASCOM Switch"
   - Select "Alpaca Switch" driver

2. **Configure Connection**:
   - **Host**: ESP32 IP address
   - **Port**: 80 (default)
   - **Device Number**: 0

3. **Switch Naming**:
   - Switches appear as "Index 0: Device Name", "Index 1: Device Name", etc.
   - Use index numbers in NINA Advanced Sequencer for reliable automation

#### Other ASCOM Software
- Use same connection parameters (IP:80, device 0)
- Compatible with SGP, MaxIm DL, TheSkyX, etc.
- Supports standard ASCOM Switch interface

### Web Interface Features

#### Main Configuration Page
- **Device List**: Shows all discovered Kasa switches with details
- **Toggle Controls**: Enable/disable individual switches
- **Status Display**: Shows enabled vs. total device count

#### Control Buttons
- **Discover Kasa Devices**: Scan network for new devices
- **Re-check Saved**: Verify saved devices are still accessible  
- **Save**: Persist current configuration to flash memory

### Network Architecture

```
[NINA/ASCOM Client] ←→ [ESP32 Alpaca Server] ←→ [Kasa Smart Switches]
     (WiFi/HTTP)              (WiFi)              (Kasa Protocol)
```

## Advanced Configuration

### Device Limits
- **Maximum Switches**: 15 devices can be discovered and configured
- **Exposed Switches**: Only enabled switches count toward ASCOM MaxSwitch
- **Memory Usage**: ~200 bytes per configured switch

### Debug Mode
Enable detailed logging by editing `src/Switch.h`:
```cpp
#define DEBUG_SWITCH  // Uncomment this line
```

### Custom Network Settings
Modify discovery timeouts in `Switch.cpp` if needed:
```cpp
const unsigned long discovery_timeout = 5500;  // Network scan duration (ms)
const unsigned long broadcast_interval = 900;   // Broadcast frequency (ms)
```

## Troubleshooting

### Common Issues

#### ESP32 Won't Connect to WiFi
- Verify WiFi credentials in `Config.h`
- Check 2.4GHz network (ESP32 doesn't support 5GHz)
- Ensure network allows device-to-device communication

#### Devices Not Discovered
- Confirm Kasa devices are on same network as ESP32
- Check devices work in Kasa mobile app
- Try power cycling Kasa devices
- Verify router allows UDP broadcasts (port 9999)

#### NINA Connection Failed
- Verify ESP32 IP address is correct
- Use port 80 (not 8080 or others)  
- Check firewall settings on computer
- Ensure ESP32 and computer are on same network

#### Switches Don't Respond
- Use "Re-check Saved" to verify device connectivity
- Check device IP addresses haven't changed (DHCP)
- Verify devices aren't in use by Kasa app simultaneously

### Debug Information
Enable debug mode for detailed logging:
1. Uncomment `#define DEBUG_SWITCH` in `Switch.h`
2. Recompile and upload
3. Monitor serial output at 115200 baud
4. Debug logs show device communication details

## Project Structure

```
ESP32AlpacaKasaSwitch/
├── src/
│   ├── main.cpp           # Main program entry
│   ├── Switch.cpp         # Kasa switch implementation  
│   ├── Switch.h           # Switch class definition
│   └── Config.h           # WiFi and system configuration
├── lib/
│   ├── ESP32AlpacaDevices/  # ASCOM Alpaca library
│   └── SLog-main/           # Logging library
├── data/
│   └── www/                 # Web interface files
├── doc/                     # Documentation and test results
└── platformio.ini          # Build configuration
```

## Version History

- **v2.1.7**: Performance optimizations, debug control improvements
- **v2.1.6**: Fixed key matching issues in web interface  
- **v2.1.5**: Enhanced JavaScript parsing and stable key handling
- **v2.2.0**: Added switch selection and persistent storage features
- **v2.1.1**: Alphabetical device sorting for consistent indexing

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project builds upon the ESP32AlpacaDevices library by Peter N (@BigPet). Please refer to the original library documentation for licensing terms.

## Support

For issues and questions:
1. Check the troubleshooting section above
2. Review existing GitHub issues
3. Create a new issue with detailed description and debug logs
4. Include your hardware setup and network configuration

## Related Links

- [ASCOM Standards](https://ascom-standards.org/)
- [NINA Imaging Software](https://nighttime-imaging.eu/)
- [TP-Link Kasa Devices](https://www.tp-link.com/us/kasa-smart/)
- [ESP32AlpacaDevices Library](https://github.com/elenhinan/ESPAscomAlpacaServer) (Original inspiration)

---

**Happy Imaging! 🌟**

*Transform your observatory with automated switch control through this ESP32-based ASCOM Alpaca solution.*