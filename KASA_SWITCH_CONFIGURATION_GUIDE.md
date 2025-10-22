# Kasa Switch Configuration Guide

## Overview
This version supports enabling/disabling individual Kasa smart plugs through a web-based configuration interface. It now saves the selected devices to ESP32 NVS (Preferences) so subsequent boots do not rescan the network; only the enabled devices are exposed to Alpaca clients.

## Key Features

### 1. **Discovery and Configuration Separation**
- **Discovery**: All Kasa devices on the network are discovered and stored
- **Configuration**: Only enabled switches are made available through Alpaca interface
- **Persistence**: Selected devices and enable states are saved in ESP32 NVS (Preferences)

### 2. **Web-Based Configuration Interface**
- Enhanced setup.html with dedicated "Kasa Switches" tab
- Real-time enable/disable toggle switches
- Shows enabled switches with their descriptive names
- Displays device information (name, IP, model, type)

### 3. **Switch Management**
- Alpaca switch indices (0, 1, 2, ...) are automatically assigned to enabled switches
- Switches are presented with their descriptive names for easy identification
- Disabled switches don't consume Alpaca indices

## How It Works

### Discovery Process
1. **Network Scan (on demand)**: UDP broadcast discovers Kasa devices when you press "Discover Kasa Devices" in setup
2. **Device Processing**: Handles both single plugs and power strip child devices
3. **Sorting**: Devices are sorted by name for consistent ordering
4. **Storage**: All discovered devices stored in `discovered_switches` vector

### Configuration Management
1. **Settings Storage**: Full device list (IP, name, model, child info, enabled) saved in NVS
2. **Unique Keys**: Devices keyed by their stored slot; full metadata is persisted
3. **Default State**: New discoveries default to enabled until you change them
4. **On Boot**: The device list is restored from NVS; no network rescan is performed

### Alpaca Interface
1. **Active Switches**: Only enabled switches appear in `switches` vector
2. **Index Assignment**: Enabled switches get sequential Alpaca indices (0, 1, 2, ...)
3. **ASCOM Compliance**: Full Alpaca Switch v3 interface support
4. **State Sync**: Real-time synchronization with physical device states

## Using the Configuration Interface

### 1. **Access Setup Page**
```
http://[ESP32-IP-ADDRESS]/setup
```

### 2. **Navigate to Kasa Switches Tab**
- Click on "Kasa Switches" tab in the setup interface
- View all discovered devices with their details

### 3. **Configure Switches**
- **Enable/Disable**: Use toggle switches to enable/disable devices
- **View Names**: See the descriptive names of enabled switches
- **Device Info**: Review device names, IP addresses, and models

### 4. **Save Configuration**
- Click "Update" to apply changes to running configuration
- Click "Save" to persist changes to flash memory
- Changes take effect immediately for new ASCOM connections

## Configuration File Structure

Settings are stored in ESP32 NVS under the namespace `kasaswitch`:

```json
{
  "count": 3,
  "addr_0": "192.168.1.100",
  "name_0": "Living Room Lamp",
  "model_0": "KP115",
  "child_0": false,
  "cidx_0": -1,
  "devid_0": "ABC123",
  "en_0": true,
  ...
}
```

## Code Architecture Changes

### New Classes and Methods

#### **KasaPlug Class Enhancement**
```cpp
class KasaPlug {
    // ... existing members ...
    bool enabled;  // New: Track if switch is enabled
};
```

#### **Switch Class Enhancement**
```cpp
class Switch : public AlpacaSwitch {
private:
    std::vector<KasaPlug> switches;            // Active (enabled) switches
    std::vector<KasaPlug> discovered_switches; // All discovered switches
    
    // New configuration methods
    void LoadKasaSwitchSettings(const JsonObject &root);
    void SaveKasaSwitchSettings(JsonObject &root);
    void UpdateEnabledSwitches();
};
```

### Key Methods

#### **LoadKasaSwitchSettings()**
- Loads enable/disable states from saved configuration
- Applies settings to discovered switches
- Called during device initialization

#### **SaveKasaSwitchSettings()**
- Saves current enable/disable states to configuration
- Creates unique keys for each device
- Called when settings are saved

#### **UpdateEnabledSwitches()**
- Rebuilds active switches list from enabled devices
- Assigns Alpaca indices to enabled switches
- Initializes switch properties for Alpaca interface

## ASCOM Client Usage

### Index Assignment
When ASCOM clients connect, they see only enabled switches:
- **Switch 0**: First enabled switch (alphabetically)
- **Switch 1**: Second enabled switch
- **Switch N**: Nth enabled switch

### Example Scenario
**Discovered Devices:**
1. Kitchen Light (192.168.1.100) - **Enabled**
2. Living Room Lamp (192.168.1.101) - **Disabled** 
3. Bedroom Fan (192.168.1.102) - **Enabled**
4. Patio Light (192.168.1.103) - **Enabled**

**ASCOM Alpaca Interface:**
- **Switch 0**: Bedroom Fan (alphabetically first enabled)
- **Switch 1**: Kitchen Light  
- **Switch 2**: Patio Light
- **MaxSwitch**: 3

## Troubleshooting

### No Switches Discovered
1. Ensure Kasa devices are powered on
2. Verify ESP32 and Kasa devices on same network
3. Check firewall settings (UDP port 9999)
4. Try device power cycle

### Settings Not Saving
1. Check LittleFS initialization
2. Verify flash memory space
3. Check serial logs for save errors
4. Try "Save" button after "Update"

### Switch Indices Change
1. Check alphabetical ordering of enabled switches
2. Verify consistent device naming
3. Review enable/disable configuration
4. Consider using fixed device names

## Development Notes

### Adding Debug Output
Enable debugging by uncommenting in Switch.h:
```cpp
#define DEBUG_SWITCH
```

### Memory and limits
- Maximum 15 switches can be discovered/selected during setup (`kMaxKasaSwitches`)
- Each switch uses ~200 bytes memory
- On boot, only the number of enabled devices are exposed via Alpaca `MaxSwitch` (no fixed 15)

### Network Protocol
- Uses Kasa's proprietary encryption (XOR with key 171)
- TCP/UDP port 9999 for device communication
- JSON-based command structure

## Future Enhancements

Potential improvements for future versions:
1. **Custom Switch Names**: Allow renaming switches in interface
2. **Group Management**: Create groups of related switches
3. **Schedule Support**: Time-based enable/disable
4. **Device Monitoring**: Track device availability and health
5. **Backup/Restore**: Export/import configuration files