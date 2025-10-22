/**************************************************************************************************
  Filename:       Switch.cpp
  Revised:        $Date: 2025-10-21$
  Version:        Version: 2.2.0
  Description:    ASCOM Alpaca Switch Device for Kasa Smart Plugs

  Copyright 2024-2025. All rights reserved.
**************************************************************************************************/
#pragma once
#include "AlpacaSwitch.h"
#include <vector>
#include <string>

// comment/uncomment to enable/disable debugging
// #define DEBUG_SWITCH

class KasaPlug {
public:
    std::string address;
    std::string name;
    std::string model;
    bool is_child;
    int child_index;
    std::string device_id;
    bool state;
    std::string state_str;
    bool enabled;  // New: Track if this switch is enabled in configuration

    KasaPlug(const std::string& addr, const std::string& n, const std::string& m, bool child = false, int index = -1, const std::string& did = "");
    bool check(int retries = 2);
    bool turn(bool on_off);
    bool on() { return turn(true); }
    bool off() { return turn(false); }
};

class Switch : public AlpacaSwitch
{
private:
    std::vector<KasaPlug> switches;
    std::vector<KasaPlug> discovered_switches; // All discovered switches (enabled + disabled)

    // Alpaca service methods
    const bool _putAction(const char *const action, const char *const parameters, char *string_response, size_t string_response_size) { return false; }
    const bool _putCommandBlind(const char *const command, const char *const raw, bool &bool_response) { return false; }
    const bool _putCommandBool(const char *const command, const char *const raw, bool &bool_response) { return false; }
    const bool _putCommandString(const char *const command_str, const char *const raw, char *string_response, size_t string_response_size) { return false; }
    const bool _writeSwitchValue(uint32_t id, double value, SwitchAsyncType_t async_type);

    void AlpacaReadJson(JsonObject &root);
    void AlpacaWriteJson(JsonObject &root);
    
    // Custom HTTP endpoints
    void _handleDiscoverKasa(AsyncWebServerRequest *request);
    
    // Helper methods for configuration management
    void LoadKasaSwitchSettings(const JsonObject &root);
    void SaveKasaSwitchSettings(JsonObject &root);
    void LoadKasaSwitchSettingsFromPersistentStorage();
    void SaveKasaSwitchSettingsToPersistentStorage();
    void UpdateEnabledSwitches();
    void InitializeSwitchesFromMemory();

#ifdef DEBUG_SWITCH
    void DebugSwitchDevice(uint32_t id);
#endif

private:
    uint32_t enabledSwitchCount = 0;  // Track enabled switch count

public:
    Switch();
    void Begin();
    void Loop();
    void Discover();
    // Expose only enabled switch count to Alpaca clients
    using AlpacaSwitch::SetMaxSwitchDevices;
};