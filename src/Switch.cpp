/**************************************************************************************************
  Filename:       Switch.cpp
  Revised:        $Date: 2025-10-21$
  Version:        Version: 2.2.0
  Description:    ASCOM Alpaca Switch Device for Kasa Smart Plugs

  Copyright 2024-2025. All rights reserved.
**************************************************************************************************/
#include "Switch.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <SLog.h>
#include <Preferences.h>
#include <map>

// Maximum number of switches selectable during discovery UI; exposed count will match enabled
const size_t kMaxKasaSwitches = 15; // hard upper bound for allocation and UI

std::string encrypt(const std::string& input) {
    std::string result;
    uint8_t key = 171;
    for (char c : input) {
        uint8_t a = key ^ static_cast<uint8_t>(c);
        key = a;
        result += static_cast<char>(a);
    }
    return result;
}

std::string decrypt(const std::string& input) {
    std::string result;
    uint8_t key = 171;
    for (char c : input) {
        uint8_t a = key ^ static_cast<uint8_t>(c);
        key = static_cast<uint8_t>(c);
        result += static_cast<char>(a);
    }
    return result;
}

int send_query(const std::string& ip, JsonDocument& query_doc, JsonDocument& response_doc, int retries = 3) {
    String payload;
    serializeJson(query_doc, payload);
    std::string enc = encrypt(payload.c_str());

    uint32_t len = htonl(static_cast<uint32_t>(enc.size()));
    std::string message(reinterpret_cast<char*>(&len), 4);
    message += enc;

    for (int attempt = 0; attempt < retries; ++attempt) {
        WiFiClient client;
        client.setTimeout(2000);
        if (!client.connect(ip.c_str(), 9999)) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Attempt %d: Failed to connect to %s:9999\n", attempt + 1, ip.c_str());
#endif
            if (attempt < retries - 1) delay(500);
            continue;
        }

        client.write(reinterpret_cast<const uint8_t*>(message.c_str()), message.size());

        uint8_t buf[4];
        int read_bytes = client.readBytes(buf, 4);
        if (read_bytes != 4) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Attempt %d: Failed to read length from %s: got %d bytes\n", attempt + 1, ip.c_str(), read_bytes);
#endif
            client.stop();
            if (attempt < retries - 1) delay(500);
            continue;
        }

        uint32_t rlen = ntohl(*reinterpret_cast<uint32_t*>(buf));
        if (rlen > 4096) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Attempt %d: Response length from %s too large: %u bytes\n", attempt + 1, ip.c_str(), rlen);
#endif
            client.stop();
            return 5;
        }

        std::vector<uint8_t> renc_vec(rlen);
        size_t received = 0;
        unsigned long start = millis();
        while (received < rlen && millis() - start < 2000) {
            int got = client.read(&renc_vec[received], rlen - received);
            if (got < 0) {
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("Attempt %d: Read error from %s: %d\n", attempt + 1, ip.c_str(), got);
#endif
                client.stop();
                return 5;
            }
            received += got;
        }

        if (received != rlen) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Attempt %d: Incomplete read from %s: got %zu of %u bytes\n", attempt + 1, ip.c_str(), received, rlen);
#endif
            client.stop();
            if (attempt < retries - 1) delay(500);
            continue;
        }

        std::string renc(reinterpret_cast<char*>(renc_vec.data()), rlen);
        std::string rplain = decrypt(renc);

        client.stop();

        DeserializationError error = deserializeJson(response_doc, rplain);
        if (error) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("JSON parse error from %s: %s\nRaw response: %s\n", ip.c_str(), error.c_str(), rplain.c_str());
#endif
            if (attempt < retries - 1) delay(500);
            continue;
        }

        if (response_doc["error_code"].is<int>() && response_doc["error_code"].as<int>() != 0) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Kasa error from %s: error_code %d\n", ip.c_str(), response_doc["error_code"].as<int>());
#endif
            return 7;
        }

        return 0;
    }

    return 2;
}

KasaPlug::KasaPlug(const std::string& addr, const std::string& n, const std::string& m, bool child, int index, const std::string& did)
    : address(addr), name(n), model(m), is_child(child), child_index(index), device_id(did), state(false), state_str("off"), enabled(true) {
#ifdef DEBUG_SWITCH
    SLOG_DEBUG_PRINTF("Created KasaPlug: %s, is_child: %d, child_index: %d, device_id: %s, enabled: %d\n",
                      name.c_str(), is_child, child_index, device_id.c_str(), enabled);
#endif
}

bool KasaPlug::check(int retries) {
    JsonDocument query_doc;
    JsonObject query = query_doc.to<JsonObject>();
    query["system"].to<JsonObject>()["get_sysinfo"] = JsonObject();

    std::string full_child_id;
    if (is_child && child_index >= 0) {
        std::string index_str = child_index < 10 ? "0" + std::to_string(child_index) : std::to_string(child_index);
        full_child_id = device_id + index_str;
        JsonObject context = query["context"].to<JsonObject>();
        JsonArray child_ids = context["child_ids"].to<JsonArray>();
        child_ids.add(full_child_id.c_str());
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Querying child plug %s with child_index %d, full_child_id %s\n",
                          name.c_str(), child_index, full_child_id.c_str());
#endif
    }

    JsonDocument resp_doc;
    int result = send_query(address, query_doc, resp_doc, retries);
    if (result != 0) {
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Check failed for %s: error code %d\n", name.c_str(), result);
#endif
        return false;
    }

    JsonObject sysinfo = resp_doc["system"]["get_sysinfo"];
    if (sysinfo.isNull()) {
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("No sysinfo in response for %s\n", name.c_str());
#endif
        return false;
    }

    if (is_child && child_index >= 0) {
        JsonArray children = sysinfo["children"];
        if (children.isNull() || child_index >= (int)children.size()) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("No children array or invalid child_index %d for %s\n", child_index, name.c_str());
#endif
            return false;
        }
        JsonObject child = children[child_index];
        std::string child_id_from_response = child["id"].as<std::string>();
        if (child_id_from_response != full_child_id) {
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Child ID mismatch for %s: expected %s, got %s\n",
                              name.c_str(), full_child_id.c_str(), child_id_from_response.c_str());
#endif
            return false;
        }
        state = child["state"].as<int>() == 1;
    } else {
        state = sysinfo["relay_state"].as<int>() == 1;
    }
    state_str = state ? "on" : "off";
    return true;
}

bool KasaPlug::turn(bool on_off) {
    int st = on_off ? 1 : 0;
    JsonDocument query_doc;
    JsonObject query = query_doc.to<JsonObject>();
    JsonObject system = query["system"].to<JsonObject>();
    JsonObject set_relay = system["set_relay_state"].to<JsonObject>();
    set_relay["state"] = st;

    if (is_child && child_index >= 0) {
        std::string index_str = child_index < 10 ? "0" + std::to_string(child_index) : std::to_string(child_index);
        std::string full_child_id = device_id + index_str;
        JsonObject context = query["context"].to<JsonObject>();
        JsonArray child_ids = context["child_ids"].to<JsonArray>();
        child_ids.add(full_child_id.c_str());
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Turning %s %s with child_index %d, full_child_id %s\n",
                          name.c_str(), on_off ? "ON" : "OFF", child_index, full_child_id.c_str());
#endif
    }

    JsonDocument resp_doc;
    if (send_query(address, query_doc, resp_doc) != 0) {
        return false;
    }
    return check();
}

Switch::Switch() : AlpacaSwitch(kMaxKasaSwitches) {
    // Initialize all switches up to kMaxKasaSwitches with default "Disabled" values
    for (size_t u = 0; u < kMaxKasaSwitches; u++) {
        char name[32];
        snprintf(name, sizeof(name), "Disabled_%zu", u);
        InitSwitchName(u, name);
        InitSwitchDescription(u, "Disabled Kasa Plug");
        InitSwitchCanWrite(u, false);
        InitSwitchMinValue(u, 0.0);
        InitSwitchMaxValue(u, 0.0);
        InitSwitchStep(u, 0.0);
        InitSwitchCanAsync(u, SwitchAsyncType_t::kNoAsyncType);
        InitSwitchInitBySetup(u, false);
        SetSwitchValue(u, 0.0);
    }
}

void Switch::Begin() {
    SLOG_INFO_PRINTF("Switch::Begin() starting...\n");
    
    // Load saved switches from persistent storage first
    SLOG_INFO_PRINTF("Loading settings from persistent storage...\n");
    LoadKasaSwitchSettingsFromPersistentStorage();
    
    // Initialize switches based on what's saved in memory (no network discovery)
    SLOG_INFO_PRINTF("Initializing switches from memory...\n");
    InitializeSwitchesFromMemory();
    // Expose only enabled switches to clients before base initialization
    SetMaxSwitchDevices(enabledSwitchCount);
    
    SLOG_INFO_PRINTF("Calling AlpacaSwitch::Begin()...\n");
    AlpacaSwitch::Begin();
    
    // TODO: Custom HTTP endpoint disabled due to crash - will use alternative approach
    // this->createCallBack(LHF(_handleDiscoverKasa), HTTP_POST, "discover_kasa");
    
    // MaxSwitch already set to enabled count

#ifdef DEBUG_SWITCH
    DebugSwitchDevice(kMaxKasaSwitches);
#endif
    
    SLOG_INFO_PRINTF("Switch::Begin() completed successfully\n");
}

void Switch::Loop() {
    for (size_t u = 0; u < switches.size(); u++) {
        if (switches[u].check()) {
            SetSwitchValue(u, switches[u].state ? 1.0 : 0.0);
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Updated switch %zu: %s, state: %s\n", u, switches[u].name.c_str(), switches[u].state_str.c_str());
#endif
        }
    }
}

void Switch::Discover() {
    discovered_switches.clear();
    switches.clear();
    SLOG_INFO_PRINTF("Discovering Kasa smart plugs...\n");

    std::vector<KasaPlug> temp_switches;

    WiFiUDP udp;
    udp.begin(0);
    std::string disc_json = "{\"system\":{\"get_sysinfo\":{}}}";
    std::string enc = encrypt(disc_json);
    unsigned long start = millis();
    unsigned long last_broadcast = 0;
    const unsigned long discovery_timeout = 5500;  // Slightly longer to catch stragglers
    const unsigned long broadcast_interval = 900;  // A bit more frequent broadcasts
    const unsigned long max_process_time = 80;     // Process packets longer each iteration

    while (millis() - start < discovery_timeout) {
        unsigned long loop_start = millis();
        
        // Feed the watchdog at start of each loop
        yield();
        delay(10); // Reduced cooperative delay
        
        if (millis() - last_broadcast >= broadcast_interval) {
            // Yield before network operation
            yield();
            udp.beginPacket("255.255.255.255", 9999);
            udp.write(reinterpret_cast<const uint8_t*>(enc.c_str()), enc.size());
            udp.endPacket();
            // Yield after network operation
            yield();
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Sent discovery broadcast\n");
#endif
            last_broadcast = millis();
        }

        // Process packets for max 50ms per loop iteration
    while ((millis() - loop_start < max_process_time) && (millis() - start < discovery_timeout)) {
            // Yield frequently during packet processing
            yield();
            
            int len = udp.parsePacket();
            if (len <= 0) {
                delay(5); // Very short delay if no packet
                continue;
            }
            
            std::vector<char> buf(len + 1);
            udp.read(buf.data(), len);
            buf[len] = '\0';
            // Yield after UDP read
            yield();
            
            std::string renc(buf.data(), len);
            std::string rplain = decrypt(renc);
            // Yield after decryption
            yield();

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, rplain);
            if (error) {
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("Discovery JSON parse error: %s\nRaw response: %s\n", error.c_str(), rplain.c_str());
#endif
                continue;
            }
            // Yield after JSON parsing
            yield();

            JsonObject sysinfo = doc["system"]["get_sysinfo"];
            if (sysinfo.isNull()) {
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("No sysinfo in response\n");
#endif
                continue;
            }

            // Yield before intensive processing
            yield();
            
            std::string alias = sysinfo["alias"].as<std::string>();
            std::string model = sysinfo["model"].as<std::string>();
            IPAddress remote = udp.remoteIP();
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", remote[0], remote[1], remote[2], remote[3]);
            std::string host = ip_str;
            std::string dev_id = sysinfo["deviceId"].as<std::string>();

            // Yield during duplicate check
            yield();
            
            bool is_duplicate = false;
            for (const auto& device : temp_switches) {
                if (device.address == host && device.name == alias) {
                    is_duplicate = true;
                    break;
                }
            }
            if (is_duplicate) {
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("Skipping duplicate device: %s at %s\n", alias.c_str(), host.c_str());
#endif
                continue;
            }

            if (sysinfo["children"].is<JsonArray>()) {
                JsonArray children = sysinfo["children"];
                for (size_t idx = 0; idx < children.size() && temp_switches.size() < kMaxKasaSwitches; ++idx) {
                    // Feed watchdog very frequently during child processing
                    yield();
                    delay(1); // Minimal delay for each child
                    
                    JsonObject child = children[idx];
                    std::string child_alias = child["alias"].as<std::string>();
                    
                    // Yield before duplicate check
                    yield();
                    
                    bool child_duplicate = false;
                    for (const auto& device : temp_switches) {
                        if (device.address == host && device.name == child_alias && device.is_child && device.child_index == idx) {
                            child_duplicate = true;
                            break;
                        }
                    }
                    if (child_duplicate) {
#ifdef DEBUG_SWITCH
                        SLOG_DEBUG_PRINTF("Skipping duplicate child plug: %s at %s, index %zu\n", child_alias.c_str(), host.c_str(), idx);
#endif
                        continue;
                    }
                    temp_switches.push_back(KasaPlug(host, child_alias, model, true, idx, dev_id));
#ifdef DEBUG_SWITCH
                    SLOG_DEBUG_PRINTF("Discovered child plug: %s, child_index: %zu, device_id: %s, IP: %s\n",
                                      child_alias.c_str(), idx, dev_id.c_str(), host.c_str());
#endif
                }
            } else if (temp_switches.size() < kMaxKasaSwitches) {
                temp_switches.push_back(KasaPlug(host, alias, model));
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("Discovered single plug: %s, device_id: %s, IP: %s\n",
                                  alias.c_str(), dev_id.c_str(), host.c_str());
#endif
            }
            
            // Yield after processing this device
            yield();
        }
        
        // End of packet processing loop - yield before continuing
        yield();
        delay(5); // Reduced delay before next main loop iteration
    }

    // Send one last discovery broadcast to catch late responders
    udp.beginPacket("255.255.255.255", 9999);
    udp.write(reinterpret_cast<const uint8_t*>(enc.c_str()), enc.size());
    udp.endPacket();
    unsigned long tail_start = millis();
    while (millis() - tail_start < 400) {
        int len = udp.parsePacket();
        if (len <= 0) {
            delay(5);
            continue;
        }
        std::vector<char> buf(len + 1);
        udp.read(buf.data(), len);
        buf[len] = '\0';
        std::string renc(buf.data(), len);
        std::string rplain = decrypt(renc);
        JsonDocument doc;
        if (deserializeJson(doc, rplain)) {
            continue;
        }
        JsonObject sysinfo = doc["system"]["get_sysinfo"];
        if (sysinfo.isNull()) continue;
        std::string alias = sysinfo["alias"].as<std::string>();
        std::string model = sysinfo["model"].as<std::string>();
        IPAddress remote = udp.remoteIP();
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", remote[0], remote[1], remote[2], remote[3]);
        std::string host = ip_str;
        std::string dev_id = sysinfo["deviceId"].as<std::string>();
        bool duplicate = false;
        for (const auto& d : temp_switches) { if (d.address == host && d.name == alias) { duplicate = true; break; } }
        if (duplicate) continue;
        if (sysinfo["children"].is<JsonArray>()) {
            JsonArray children = sysinfo["children"];
            for (size_t idx = 0; idx < children.size() && temp_switches.size() < kMaxKasaSwitches; ++idx) {
                JsonObject child = children[idx];
                std::string child_alias = child["alias"].as<std::string>();
                bool child_dup = false;
                for (const auto& d : temp_switches) {
                    if (d.address == host && d.name == child_alias && d.is_child && d.child_index == (int)idx) { child_dup = true; break; }
                }
                if (!child_dup) temp_switches.push_back(KasaPlug(host, child_alias, model, true, (int)idx, dev_id));
            }
        } else if (temp_switches.size() < kMaxKasaSwitches) {
            temp_switches.push_back(KasaPlug(host, alias, model));
        }
    }

    // Feed watchdog before final processing
    yield();

    // Sort temp_switches by name
    std::sort(temp_switches.begin(), temp_switches.end(), [](const KasaPlug& a, const KasaPlug& b) {
        return a.name < b.name;
    });

    // Feed watchdog after sorting
    yield();

    // Store all discovered switches
    discovered_switches = std::move(temp_switches);
    
    // Feed watchdog after assignment
    yield();
    
    // For fresh discovery via web interface, enable ALL devices by default
    // Do NOT load saved settings - user can configure manually via web interface
    for (auto& plug : discovered_switches) {
        plug.enabled = true;
        SLOG_INFO_PRINTF("Setting %s to enabled by default\n", plug.name.c_str());
    }
    
    // Feed watchdog after enabling all
    yield();
    
    // Update enabled switches based on current configuration
    UpdateEnabledSwitches();
    // Adjust exposed count to the enabled subset
    SetMaxSwitchDevices(enabledSwitchCount);

    // Feed watchdog after updating switches
    yield();

    udp.stop();
#ifdef DEBUG_SWITCH
    SLOG_DEBUG_PRINTF("UDP discovery closed\n");
#endif
    SLOG_INFO_PRINTF("Found %d Kasa switches (enabled) out of %d discovered\n", static_cast<int>(switches.size()), static_cast<int>(discovered_switches.size()));
}

const bool Switch::_writeSwitchValue(uint32_t id, double value, SwitchAsyncType_t async_type) {
    if (id >= switches.size()) {
        SLOG_DEBUG_PRINTF("Invalid switch ID: %u\n", id);
        return false;
    }

    bool target_state = value > 0.5;
    bool result = switches[id].turn(target_state);
    if (result) {
        SetSwitchValue(id, target_state ? 1.0 : 0.0);
        SetStateChangeComplete(id, true);
    }

#ifdef DEBUG_SWITCH
    SLOG_DEBUG_PRINTF("id=%u async_type=%s value=%f result=%s\n",
                      id, async_type == SwitchAsyncType_t::kAsyncType ? "true" : "false",
                      value, result ? "true" : "false");
#endif
    return result;
}

void Switch::AlpacaReadJson(JsonObject &root) {
    DBG_JSON_PRINTFJ(SLOG_NOTICE, root, "BEGIN (root=<%s>) ...\n", _ser_json_);
    AlpacaSwitch::AlpacaReadJson(root);

    // Check for discovery trigger
    bool discoveryTrigger = root["KasaDiscoveryTrigger"].as<bool>();
    SLOG_INFO_PRINTF("Checking discovery trigger: %s\n", discoveryTrigger ? "true" : "false");
    
    if (discoveryTrigger) {
        SLOG_INFO_PRINTF("Discovery trigger received - starting Kasa device discovery...\n");
        Discover();
        SLOG_INFO_PRINTF("Discovery completed - found %d switches\n", static_cast<int>(discovered_switches.size()));
        return; // Exit early after discovery
    }

    // Re-check saved devices without discovery (quick presence check)
    bool recheckSaved = root["KasaRecheckSaved"].as<bool>();
    if (recheckSaved) {
        SLOG_INFO_PRINTF("Re-check saved devices trigger received - reloading from storage and validating...\n");
        LoadKasaSwitchSettingsFromPersistentStorage();
        InitializeSwitchesFromMemory();
        SetMaxSwitchDevices(enabledSwitchCount);
        SLOG_INFO_PRINTF("Re-check completed - %d enabled and reachable switches\n", static_cast<int>(enabledSwitchCount));
        return;
    }

    // Process toggle changes from web interface
    // Prefer robust array of enabled stable keys if present
    JsonArray enabled_keys = root["KasaEnabledKeys"]; // array of stable keys
#ifdef DEBUG_SWITCH
    SLOG_DEBUG_PRINTF("KasaEnabledKeys: isNull=%s, size=%u\n", 
                      enabled_keys.isNull() ? "true" : "false", 
                      enabled_keys.isNull() ? 0 : static_cast<unsigned>(enabled_keys.size()));
#endif
    if (!enabled_keys.isNull() && enabled_keys.size() > 0) {
        SLOG_INFO_PRINTF("Applying KasaEnabledKeys (%u items)\n", static_cast<unsigned>(enabled_keys.size()));
        // Build a fast lookup of enabled keys
        std::vector<std::string> enabled_list;
        enabled_list.reserve(enabled_keys.size());
        for (JsonVariant v : enabled_keys) {
            if (v.is<const char*>()) {
                const char* s = v.as<const char*>();
                if (s && *s) enabled_list.emplace_back(s);
            }
        }
        
        // Debug: Log the received enabled keys
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Received enabled keys: ");
        for (const auto& key : enabled_list) {
            SLOG_DEBUG_PRINTF("'%s' ", key.c_str());
        }
        SLOG_DEBUG_PRINTF("\n");
#endif
        auto is_enabled = [&enabled_list](const std::string &key) -> bool {
            for (const auto &k : enabled_list) {
                if (k == key) return true;
            }
            return false;
        };

        bool settings_changed = false;
        for (auto &plug : discovered_switches) {
            std::string stable_key = plug.address + "_" + plug.name;
            if (plug.is_child) {
                stable_key += "_child_" + std::to_string(plug.child_index);
            }
            
            // Clean up stable_key to match AlpacaWriteJson() format exactly
            // IMPORTANT: Replace all special characters with underscores to match form data format
            for (char& c : stable_key) {
                if (c == ' ' || c == '-' || c == '(' || c == ')' || c == '.') {
                    c = '_';
                }
            }
            
            // Remove trailing underscores from stable_key
            while (!stable_key.empty() && stable_key.back() == '_') {
                stable_key.pop_back();
            }
            
            // Ensure stable_key is not empty
            if (stable_key.empty()) {
                stable_key = "default_key";
            }
            
            bool new_enabled_state = is_enabled(stable_key);
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Checking %s: generated_key='%s' enabled=%s\n", 
                              plug.name.c_str(), stable_key.c_str(), new_enabled_state ? "true" : "false");
#endif
            if (plug.enabled != new_enabled_state) {
                plug.enabled = new_enabled_state;
                settings_changed = true;
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("Changed %s from %s to %s\n", 
                                  plug.name.c_str(), 
                                  !new_enabled_state ? "enabled" : "disabled",
                                  new_enabled_state ? "enabled" : "disabled");
#endif
            }
        }
        UpdateEnabledSwitches();
        SaveKasaSwitchSettingsToPersistentStorage();
        SLOG_INFO_PRINTF("KasaEnabledKeys applied (%s)\n", settings_changed ? "changed" : "no-change");
        return; // handled via robust path
    }

    JsonObject kasa_selection = root["KasaSwitchSelection"];
    JsonObject kasa_key_map = root["_KasaSwitchKeyMapHidden"]; // preferred hidden name
    // Accept legacy visible name if posted
    if (!kasa_key_map) {
        JsonVariant legacyMap = root["KasaSwitchKeyMap"]; // deprecated containsKey() replaced
        if (!legacyMap.isNull() && legacyMap.is<JsonObject>()) {
            kasa_key_map = legacyMap.as<JsonObject>();
        }
    }
    if (kasa_selection) {
        bool settings_changed = false;
        // Determine if the posted selection is partial (missing keys)
        size_t posted_count = 0;
        for (JsonPair kv : kasa_selection) {
            (void)kv;
            posted_count++;
        }
        bool default_missing_to_false = posted_count > 0 && posted_count < discovered_switches.size();
        if (default_missing_to_false) {
            SLOG_INFO_PRINTF("KasaSwitchSelection appears partial (%u of %u); missing entries will default to disabled\n",
                             static_cast<unsigned>(posted_count), static_cast<unsigned>(discovered_switches.size()));
        } else {
            SLOG_INFO_PRINTF("KasaSwitchSelection posted with %u entries (discovered=%u)\n",
                             static_cast<unsigned>(posted_count), static_cast<unsigned>(discovered_switches.size()));
        }

        // Debug: Log what keys were actually posted
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Posted keys: ");
        for (JsonPair kv : kasa_selection) {
            SLOG_DEBUG_PRINTF("%s=%s ", kv.key().c_str(), kv.value().as<String>().c_str());
        }
        SLOG_DEBUG_PRINTF("\n");
#endif
        
        // Check each discovered switch for changes
        for (size_t i = 0; i < discovered_switches.size(); ++i) {
            const auto& plug = discovered_switches[i];
            
            // Create the same clean key format as in AlpacaWriteJson
            char switch_key[32];
            snprintf(switch_key, sizeof(switch_key), "%s", plug.name.c_str());
            
            // Replace spaces and special characters with underscores
            for (char* p = switch_key; *p; ++p) {
                if (*p == ' ' || *p == '-' || *p == '(' || *p == ')' || *p == '.') {
                    *p = '_';
                }
            }
            
            // Truncate to same length as in AlpacaWriteJson
            if (strlen(switch_key) > 20) {
                switch_key[20] = '\0';
            }
            
            // Remove trailing underscores to prevent JavaScript parsing issues
            size_t len = strlen(switch_key);
            while (len > 0 && switch_key[len - 1] == '_') {
                switch_key[len - 1] = '\0';
                len--;
            }
            
            // Ensure we don't have an empty key
            if (strlen(switch_key) == 0) {
                snprintf(switch_key, sizeof(switch_key), "sw%zu", i);
            }
            
            // Prefer the stable key via the posted key map
            String stable_lookup = kasa_key_map[switch_key] | "";
            JsonVariant v = kasa_selection[switch_key];
            if (stable_lookup.length() > 0) {
                // If client sent stable keys as direct fields, support that too
                JsonVariant vs = kasa_selection[stable_lookup.c_str()];
                if (!vs.isNull()) v = vs;
            }
            bool have_value = !v.isNull();
            bool new_enabled_state = discovered_switches[i].enabled;
            if (have_value) {
                if (v.is<bool>()) {
                    new_enabled_state = v.as<bool>();
                } else if (v.is<int>()) {
                    new_enabled_state = v.as<int>() != 0;
                } else if (v.is<float>()) {
                    new_enabled_state = v.as<float>() > 0.5f;
                } else if (v.is<const char*>()) {
                    const char* s = v.as<const char*>();
                    if (s) {
                        if (strcasecmp(s, "true") == 0 || strcasecmp(s, "on") == 0 || strcmp(s, "1") == 0) new_enabled_state = true;
                        else if (strcasecmp(s, "false") == 0 || strcasecmp(s, "off") == 0 || strcmp(s, "0") == 0) new_enabled_state = false;
                    }
                }
            } else if (default_missing_to_false) {
                new_enabled_state = false;
            }

            if (discovered_switches[i].enabled != new_enabled_state) {
                discovered_switches[i].enabled = new_enabled_state;
                settings_changed = true;
#ifdef DEBUG_SWITCH
                SLOG_DEBUG_PRINTF("Updated switch %s enabled state to: %d\n", 
                                  discovered_switches[i].name.c_str(), discovered_switches[i].enabled);
#endif
            }
        }
        
        // Update the active switches list based on new settings
        UpdateEnabledSwitches();
        // Persist regardless of change detection to be robust against UI/session mismatches
    SaveKasaSwitchSettingsToPersistentStorage();
    SLOG_INFO_PRINTF("Kasa switch settings saved (%s); enabled now=%u of %u\n",
             settings_changed ? "changed" : "no-change",
             static_cast<unsigned>(enabledSwitchCount),
             static_cast<unsigned>(discovered_switches.size()));
    }

    char title[32];
    for (size_t u = 0; u < kMaxKasaSwitches; u++) {
        snprintf(title, sizeof(title), "Configuration_Device_%zu", u);
        if (JsonObject obj_config = root[title]) {
            InitSwitchName(u, obj_config["Name"] | GetSwitchName(u));
            InitSwitchDescription(u, obj_config["Description"] | GetSwitchDescription(u));
            InitSwitchCanWrite(u, obj_config["CanWrite"] | GetSwitchCanWrite(u));
            InitSwitchMinValue(u, obj_config["MinValue"] | GetSwitchMinValue(u));
            InitSwitchMaxValue(u, obj_config["MaxValue"] | GetSwitchMaxValue(u));
            InitSwitchStep(u, obj_config["Step"] | GetSwitchStep(u));
            DBG_JSON_PRINTFJ(SLOG_NOTICE, obj_config, "... title=%s obj_config=<%s> \n", title, _ser_json_);
        }
    }
    SLOG_PRINTF(SLOG_NOTICE, "... END\n");
}

void Switch::AlpacaWriteJson(JsonObject &root) {
    DBG_JSON_PRINTFJ(SLOG_NOTICE, root, "BEGIN root=%s ...\n", _ser_json_);
    
    // Only add Kasa Switch Selection section if there are discovered switches
    if (discovered_switches.size() > 0) {
        JsonObject kasa_selection = root["KasaSwitchSelection"].to<JsonObject>();
        // Write the key map under a hidden property name so the UI doesn't render it
        JsonObject kasa_key_map = root["_KasaSwitchKeyMapHidden"].to<JsonObject>();
        // Provide a robust array of currently enabled stable keys for clients to POST back
        JsonArray enabled_keys = root["KasaEnabledKeys"].to<JsonArray>();
        
        for (size_t i = 0; i < discovered_switches.size(); ++i) {
        const auto& plug = discovered_switches[i];
        
        // Create a clean field name using just the switch name
        char switch_key[32];
        snprintf(switch_key, sizeof(switch_key), "%s", plug.name.c_str());
        
        // Replace spaces and special characters with underscores, but keep it shorter
        for (char* p = switch_key; *p; ++p) {
            if (*p == ' ' || *p == '-' || *p == '(' || *p == ')' || *p == '.') {
                *p = '_';
            }
        }
        
        // Truncate very long names to prevent wrapping
        if (strlen(switch_key) > 20) {
            switch_key[20] = '\0';
        }
        
        // Remove trailing underscores to prevent JavaScript parsing issues
        size_t len = strlen(switch_key);
        while (len > 0 && switch_key[len - 1] == '_') {
            switch_key[len - 1] = '\0';
            len--;
        }
        
        // Ensure we don't have an empty key
        if (strlen(switch_key) == 0) {
            snprintf(switch_key, sizeof(switch_key), "sw%zu", i);
        }
        
            // Set the boolean value and map short key to a stable key
            kasa_selection[switch_key] = plug.enabled;
            // Stable key: address + name + optional child info
            std::string stable_key = plug.address + "_" + plug.name;
            if (plug.is_child) {
                stable_key += "_child_" + std::to_string(plug.child_index);
            }
            
            // Clean up stable_key to prevent trailing underscores and invalid characters
            // IMPORTANT: Replace dots with underscores to match what gets sent in form data
            for (char& c : stable_key) {
                if (c == ' ' || c == '-' || c == '(' || c == ')' || c == '.') {
                    c = '_';
                }
            }
            
            // Remove trailing underscores from stable_key
            while (!stable_key.empty() && stable_key.back() == '_') {
                stable_key.pop_back();
            }
            
            // Ensure stable_key is not empty
            if (stable_key.empty()) {
                stable_key = "sw" + std::to_string(i);
            }
            
            kasa_key_map[switch_key] = stable_key.c_str();
            if (plug.enabled) {
                enabled_keys.add(stable_key.c_str());
            }
        }
        
        // IMPORTANT: Also save to persistent storage (NVS) for reboot persistence
        // This ensures the "Save" button saves to both LittleFS AND NVS
        SaveKasaSwitchSettingsToPersistentStorage();
        SLOG_INFO_PRINTF("Kasa switch settings saved to both LittleFS and NVS during Save operation\n");
    } else {
        // Add a simple info object used by UI; client will show this as an info banner (not an editable field)
        JsonObject info = root["DiscoveryInfo"].to<JsonObject>();
        info["message"] = "No devices found. Click 'Discover Kasa Devices' to scan network.";
    }
    
    DBG_JSON_PRINTFJ(SLOG_NOTICE, root, "... END \"%s\"\n", _ser_json_);
}

void Switch::LoadKasaSwitchSettings(const JsonObject &root) {
    SLOG_INFO_PRINTF("Loading Kasa switch enable/disable settings...\n");
    
    JsonObject kasa_config = root["#KasaSwitchConfig"];
    if (kasa_config) {
        if (discovered_switches.empty()) {
            // No discovery yet: also load from persistent storage first
            LoadKasaSwitchSettingsFromPersistentStorage();
        }
        for (auto& discovered_plug : discovered_switches) {
            // Create a unique key for each switch (address + name + child info)
            std::string key = discovered_plug.address + "_" + discovered_plug.name;
            if (discovered_plug.is_child) {
                key += "_child_" + std::to_string(discovered_plug.child_index);
            }
            
            // Default to enabled if not found in config
            discovered_plug.enabled = kasa_config[key.c_str()] | true;
            
#ifdef DEBUG_SWITCH
            SLOG_DEBUG_PRINTF("Loaded setting for %s: enabled=%d\n", key.c_str(), discovered_plug.enabled);
#endif
        }
    }
}

void Switch::SaveKasaSwitchSettings(JsonObject &root) {
    SLOG_INFO_PRINTF("Saving Kasa switch enable/disable settings...\n");
    
    JsonObject kasa_config = root["#KasaSwitchConfig"].to<JsonObject>();
    
    for (const auto& discovered_plug : discovered_switches) {
        // Create a unique key for each switch (address + name + child info)
        std::string key = discovered_plug.address + "_" + discovered_plug.name;
        if (discovered_plug.is_child) {
            key += "_child_" + std::to_string(discovered_plug.child_index);
        }
        
        kasa_config[key.c_str()] = discovered_plug.enabled;
        
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Saved setting for %s: enabled=%d\n", key.c_str(), discovered_plug.enabled);
#endif
    }
}

void Switch::UpdateEnabledSwitches() {
    switches.clear();
    
    // Copy only enabled switches to the active switches vector
    for (const auto& discovered_plug : discovered_switches) {
        if (discovered_plug.enabled) {
            switches.push_back(discovered_plug);
        }
    }
    
    // Store enabled switch count
    enabledSwitchCount = static_cast<uint32_t>(switches.size());
    
#ifdef DEBUG_SWITCH
    SLOG_DEBUG_PRINTF("UpdateEnabledSwitches: %u enabled switches out of %zu discovered\n", 
                      enabledSwitchCount, discovered_switches.size());
#endif

    // Initialize ALL slots first as disabled
    for (size_t u = 0; u < kMaxKasaSwitches; u++) {
        char name[32];
        snprintf(name, sizeof(name), "Disabled_%zu", u);
        InitSwitchName(u, name);
        InitSwitchDescription(u, "Disabled Kasa Plug");
        InitSwitchCanWrite(u, false);
        InitSwitchMinValue(u, 0.0);
        InitSwitchMaxValue(u, 0.0);
        InitSwitchStep(u, 0.0);
        InitSwitchCanAsync(u, SwitchAsyncType_t::kNoAsyncType);
        InitSwitchInitBySetup(u, false);  // Mark as not setup
        SetSwitchValue(u, 0.0);
    }

    // Now configure only the enabled switches in the first N slots
    for (size_t id = 0; id < enabledSwitchCount; ++id) {
        auto& plug = switches[id];
        bool is_child = plug.is_child;
        
        // Use plain switch name without index prefix
        std::string desc = std::string("Kasa ") + (is_child ? "Child " : "") + "Plug " + plug.model;
        InitSwitchName(id, plug.name.c_str());
        InitSwitchDescription(id, desc.c_str());
        InitSwitchCanWrite(id, true);
        InitSwitchMinValue(id, 0.0);
        InitSwitchMaxValue(id, 1.0);
        InitSwitchStep(id, 1.0);
        InitSwitchCanAsync(id, SwitchAsyncType_t::kNoAsyncType);
        InitSwitchInitBySetup(id, true);  // Only enabled switches are setup
        
#ifdef DEBUG_SWITCH
        SLOG_DEBUG_PRINTF("Initialized enabled switch %zu: %s\n", id, plug.name.c_str());
#endif
    }

    // Expose only enabled switches to clients
    SetMaxSwitchDevices(enabledSwitchCount);
    
    SLOG_INFO_PRINTF("Configured %d enabled Kasa switches out of %d discovered\n", 
                     static_cast<int>(switches.size()), static_cast<int>(discovered_switches.size()));
}

void Switch::InitializeSwitchesFromMemory() {
    // Only use switches that are saved in memory - no network discovery
    // discovered_switches should already be loaded from persistent storage
    switches.clear();
    
    // Copy only enabled switches from memory to active switches and verify presence quickly
    for (const auto& saved_plug : discovered_switches) {
        if (!saved_plug.enabled) continue;
        KasaPlug tmp = saved_plug;
        bool present = tmp.check(1);
        if (present) {
            switches.push_back(tmp);
        } else {
            SLOG_NOTICE_PRINTF("Skipping unreachable saved device: %s at %s\n", saved_plug.name.c_str(), saved_plug.address.c_str());
        }
    }
    
    enabledSwitchCount = static_cast<uint32_t>(switches.size());
    
    SLOG_INFO_PRINTF("InitializeSwitchesFromMemory: Found %d enabled switches in memory\n", 
                     static_cast<int>(enabledSwitchCount));

    // Initialize ALL ASCOM switch slots as disabled first
    for (size_t u = 0; u < kMaxKasaSwitches; u++) {
        char name[32];
        snprintf(name, sizeof(name), "Disabled_%zu", u);
        InitSwitchName(u, name);
        InitSwitchDescription(u, "Disabled Kasa Plug");
        InitSwitchCanWrite(u, false);
        InitSwitchMinValue(u, 0.0);
        InitSwitchMaxValue(u, 0.0);
        InitSwitchStep(u, 0.0);
        InitSwitchCanAsync(u, SwitchAsyncType_t::kNoAsyncType);
        InitSwitchInitBySetup(u, false);
        SetSwitchValue(u, 0.0);
    }

    // Configure only the enabled switches from memory
    for (size_t id = 0; id < enabledSwitchCount; ++id) {
        auto& plug = switches[id];
        bool is_child = plug.is_child;
        
        // Use plain switch name without index prefix
        std::string desc = std::string("Kasa ") + (is_child ? "Child " : "") + "Plug " + plug.model;
        InitSwitchName(id, plug.name.c_str());
        InitSwitchDescription(id, desc.c_str());
        InitSwitchCanWrite(id, true);
        InitSwitchMinValue(id, 0.0);
        InitSwitchMaxValue(id, 1.0);
        InitSwitchStep(id, 1.0);
        InitSwitchCanAsync(id, SwitchAsyncType_t::kNoAsyncType);
        InitSwitchInitBySetup(id, true);

        // Quick presence check to ensure the device is still reachable
    bool present = plug.check(1);
        if (!present) {
            SLOG_NOTICE_PRINTF("Switch %zu (%s) not reachable at init; will still expose but state may be stale\n", id, plug.name.c_str());
        }
        
        SLOG_INFO_PRINTF("NINA will see switch %zu: %s at IP %s\n", 
                        id, plug.name.c_str(), plug.address.c_str());
    }

    // Expose only enabled switches to clients
    SetMaxSwitchDevices(enabledSwitchCount);
    SLOG_INFO_PRINTF("NINA will see %d switches from ESP32 memory\n", static_cast<int>(enabledSwitchCount));
}

void Switch::LoadKasaSwitchSettingsFromPersistentStorage() {
    Preferences prefs;
    prefs.begin("kasaswitch", true); // Open in read-only mode

    size_t count = prefs.getUInt("count", 0);
    if (count == 0) {
        SLOG_INFO_PRINTF("No saved Kasa switch settings found - all discovered devices will remain enabled\n");
        prefs.end();
        return;
    }

    SLOG_INFO_PRINTF("Loading %zu Kasa switch entries from persistent storage...\n", count);

    // If discovered_switches is empty (boot time), restore complete device list from NVS
    if (discovered_switches.empty()) {
        SLOG_INFO_PRINTF("Boot time: Restoring complete device list from NVS...\n");
        for (size_t i = 0; i < count && i < kMaxKasaSwitches; i++) {
            char key[24];

            snprintf(key, sizeof(key), "addr_%zu", i);
            String addr = prefs.getString(key, "");

            snprintf(key, sizeof(key), "name_%zu", i);
            String name = prefs.getString(key, "");

            snprintf(key, sizeof(key), "model_%zu", i);
            String model = prefs.getString(key, "");

            snprintf(key, sizeof(key), "child_%zu", i);
            bool is_child = prefs.getBool(key, false);

            snprintf(key, sizeof(key), "cidx_%zu", i);
            int child_index = prefs.getInt(key, -1);

            snprintf(key, sizeof(key), "devid_%zu", i);
            String device_id = prefs.getString(key, "");

            snprintf(key, sizeof(key), "en_%zu", i);
            bool enabled = prefs.getBool(key, true);

            if (addr.length() == 0 || name.length() == 0) {
                continue;
            }

            KasaPlug plug(addr.c_str(), name.c_str(), model.c_str(), is_child, child_index, device_id.c_str());
            plug.enabled = enabled;
            discovered_switches.push_back(plug);
            SLOG_INFO_PRINTF("Restored device %s: %s\n", plug.name.c_str(), plug.enabled ? "enabled" : "disabled");
        }
    } else {
        // Post-discovery: merge saved settings with discovered devices
        SLOG_INFO_PRINTF("Post-discovery: Merging saved settings with discovered devices...\n");
        
        // Create a map of saved settings by unique device identifier
        std::map<std::string, bool> savedEnabledStates;
        
        for (size_t i = 0; i < count && i < kMaxKasaSwitches; i++) {
            char key[24];

            snprintf(key, sizeof(key), "addr_%zu", i);
            String addr = prefs.getString(key, "");

            snprintf(key, sizeof(key), "name_%zu", i);
            String name = prefs.getString(key, "");

            snprintf(key, sizeof(key), "child_%zu", i);
            bool is_child = prefs.getBool(key, false);

            snprintf(key, sizeof(key), "cidx_%zu", i);
            int child_index = prefs.getInt(key, -1);

            snprintf(key, sizeof(key), "en_%zu", i);
            bool enabled = prefs.getBool(key, true);

            if (addr.length() == 0 || name.length() == 0) {
                continue;
            }

            // Create unique key for this device (same format as used in web interface)
            std::string stable_key = addr.c_str() + std::string("_") + name.c_str() + (is_child ? (std::string("_child_") + std::to_string(child_index)) : std::string(""));
            savedEnabledStates[stable_key] = enabled;
        }

        // Apply saved enabled states to discovered devices, leaving new devices enabled by default
        for (auto& plug : discovered_switches) {
            std::string stable_key = plug.address + "_" + plug.name + (plug.is_child ? ("_child_" + std::to_string(plug.child_index)) : "");
            
            if (savedEnabledStates.find(stable_key) != savedEnabledStates.end()) {
                plug.enabled = savedEnabledStates[stable_key];
                SLOG_INFO_PRINTF("Applied saved setting for %s: %s\n", plug.name.c_str(), plug.enabled ? "enabled" : "disabled");
            } else {
                // New device - keep enabled by default
                plug.enabled = true;
                SLOG_INFO_PRINTF("New device %s: keeping enabled by default\n", plug.name.c_str());
            }
        }
    }

    prefs.end();
}

void Switch::SaveKasaSwitchSettingsToPersistentStorage() {
    Preferences prefs;
    prefs.begin("kasaswitch", false); // Open in read-write mode

    // Clear existing settings
    prefs.clear();

    size_t count = discovered_switches.size();
    if (count > kMaxKasaSwitches) count = kMaxKasaSwitches;
    prefs.putUInt("count", count);

    SLOG_INFO_PRINTF("Saving %zu Kasa switch entries to persistent storage...\n", count);

    for (size_t i = 0; i < count; i++) {
        const auto& p = discovered_switches[i];
        char key[24];

        snprintf(key, sizeof(key), "addr_%zu", i);
        prefs.putString(key, p.address.c_str());

        snprintf(key, sizeof(key), "name_%zu", i);
        prefs.putString(key, p.name.c_str());

        snprintf(key, sizeof(key), "model_%zu", i);
        prefs.putString(key, p.model.c_str());

        snprintf(key, sizeof(key), "child_%zu", i);
        prefs.putBool(key, p.is_child);

        snprintf(key, sizeof(key), "cidx_%zu", i);
        prefs.putInt(key, p.child_index);

        snprintf(key, sizeof(key), "devid_%zu", i);
        prefs.putString(key, p.device_id.c_str());

        snprintf(key, sizeof(key), "en_%zu", i);
        prefs.putBool(key, p.enabled);
    }

    prefs.end();
    SLOG_INFO_PRINTF("Kasa switch settings saved to persistent storage\n");
}

#ifdef DEBUG_SWITCH
void Switch::DebugSwitchDevice(uint32_t id) {
    size_t tmp_id = (id == kMaxKasaSwitches) ? 0 : (id < enabledSwitchCount ? id : 0);
    size_t tmp_max = (id == kMaxKasaSwitches) ? enabledSwitchCount : tmp_id + 1;

    for (size_t u = tmp_id; u < tmp_max; u++) {
        SLOG_DEBUG_PRINTF("device_id=%zu init_by_setup=%s can_write=%s name=%s description=%s value=%lf min_value=%lf max_value=%lf step=%lf\n",
                          u,
                          GetSwitchInitBySetup(u) ? "true" : "false",
                          GetSwitchCanWrite(u) ? "true" : "false",
                          GetSwitchName(u),
                          GetSwitchDescription(u),
                          GetSwitchValue(u),
                          GetSwitchMinValue(u),
                          GetSwitchMaxValue(u),
                          GetSwitchStep(u));
    }
}
#endif

void Switch::_handleDiscoverKasa(AsyncWebServerRequest *request) {
    SLOG_INFO_PRINTF("Discovery endpoint called - starting Kasa device discovery...\n");
    
    // Run network discovery
    Discover();
    
    // Send success response
    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Discovery completed\"}");
    
    SLOG_INFO_PRINTF("Discovery endpoint completed - found %d switches\n", static_cast<int>(discovered_switches.size()));
}