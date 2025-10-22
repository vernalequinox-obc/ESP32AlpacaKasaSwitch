# Kasa Switch Selection Implementation - Version 2.2.0

## Overview
This implementation adds the requested functionality to allow users to select which Kasa devices are enabled/disabled through the Alpaca setup web interface. Only enabled devices will be visible to NINA and other ASCOM clients.

## New Features Implemented

### 1. Web Interface for Switch Selection
- **KasaSwitchToggles Section**: Added a new section in the setup web interface that displays all discovered Kasa switches
- **Index Numbers**: Each switch is shown with its index number (Index 0: Switch Name) for easy identification in NINA sequencing
- **Device Details**: Shows device model, IP address, and whether it's a child device or main device
- **Enable/Disable Toggles**: Users can enable/disable individual switches with toggle switches
- **Status Display**: Shows count of enabled vs total discovered switches

### 2. Persistent Storage
- **ESP32 Preferences**: Uses ESP32 Preferences library to save switch enable/disable settings to flash memory
- **Automatic Loading**: Settings are automatically loaded when the ESP32 starts up and during discovery
- **Automatic Saving**: Settings are saved immediately when changed through the web interface
- **Unique Identification**: Each switch is identified by a unique key combining IP address, name, and child index

### 3. Enhanced Switch Management
- **Selective Activation**: Only enabled switches are initialized as active ASCOM switches
- **Index-Based Naming**: Active switches use format "Index XX: Alias Name" for clear identification in NINA
- **Faster Response**: NINA connections will be faster as only enabled switches need to be checked/controlled
- **Discovery Order**: Switches are sorted alphabetically by name, then indexed sequentially for consistency

## Code Changes Made

### Switch.h
- Added `enabled` field to `KasaPlug` class
- Added function declarations:
  - `LoadKasaSwitchSettingsFromPersistentStorage()`
  - `SaveKasaSwitchSettingsToPersistentStorage()`

### Switch.cpp
- **Added Includes**: Added `#include <Preferences.h>` for ESP32 persistent storage
- **Modified Constructor**: `KasaPlug` constructor now initializes `enabled = true` by default
- **Enhanced AlpacaWriteJson()**: 
  - Creates `KasaSwitchToggles` section for web interface
  - Shows all discovered switches with their index numbers
  - Displays device details (model, IP, child status)
  - Shows current enabled/disabled count
- **Enhanced AlpacaReadJson()**:
  - Processes toggle changes from web interface
  - Automatically saves settings to persistent storage when changes are made
  - Calls `UpdateEnabledSwitches()` to refresh active switch list
- **Enhanced Discover()**:
  - Loads saved settings after discovery
  - Applies saved enable/disable states to discovered switches
- **Enhanced UpdateEnabledSwitches()**:
  - Creates switch names in format "Index XX: Alias Name"
  - Only initializes enabled switches as active ASCOM switches
- **New Persistent Storage Functions**:
  - `LoadKasaSwitchSettingsFromPersistentStorage()`: Loads settings from ESP32 flash memory
  - `SaveKasaSwitchSettingsToPersistentStorage()`: Saves settings to ESP32 flash memory

## User Workflow

1. **Discovery**: ESP32 discovers all Kasa devices on the network (as before)
2. **Web Configuration**: 
   - Access the ESP32 setup page in a web browser
   - Navigate to the "Kasa Switch Selection" section
   - See all discovered switches with their index numbers and details
   - Use toggle switches to enable/disable individual devices
   - Click "Update" to apply changes
   - Click "Save" to persist settings across reboots
3. **NINA Operation**:
   - Only enabled switches appear in NINA's switch list
   - Switches are named "Index XX: Device Alias" for easy identification
   - Use index numbers for NINA advanced sequencing

## Benefits

1. **Faster NINA Connection**: Only enabled switches are active, reducing connection time
2. **Observatory-Specific Setup**: Enable only the switches needed for your observatory
3. **Clear Identification**: Index numbers make it easy to select switches in NINA sequencing
4. **Persistent Configuration**: Settings survive ESP32 reboots and power cycles
5. **User-Friendly Interface**: Simple web interface for managing switch selection
6. **Backwards Compatible**: Existing functionality remains unchanged

## Technical Notes

- **Storage Location**: Settings are stored in ESP32 NVS (Non-Volatile Storage) under namespace "kasaswitch"
- **Unique Keys**: Each switch is identified by "IP_DeviceName_child_ChildIndex" for multi-outlet devices
- **Default Behavior**: Newly discovered switches default to enabled
- **Memory Usage**: Minimal storage footprint using ESP32 Preferences library
- **Error Handling**: Graceful fallback if storage operations fail

## Version Information
- **Previous Version**: 2.1.0 - Added sort after discovery to have consistent order
- **Current Version**: 2.2.0 - Added switch selection and persistent storage