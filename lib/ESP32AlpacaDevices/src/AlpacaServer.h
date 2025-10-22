/**************************************************************************************************
  Filename:       AlpacaServer.hpp
  Revised:        $Date: 2024-01-14$
  Revision:       $Revision: 01 $

  Description:    ASCOM Alpaca Server

  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
            based on https://github.com/elenhinan/ESPAscomAlpacaServer
**************************************************************************************************/
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "AlpacaDebug.h"
#include "AlpacaConfig.h"

const char kAlpacaDeviceCommand[] = "/api/v1/%s/%d/%s"; // <device_type>, <device_number>, <command>
const char kAlpacaDeviceSetup[] = "/setup/v1/%s/%d/%s"; // device_type, device_number, command

const char kAlpacaSettingsPath[] = "/settings.json";   // Path to server and device settings
const char kAlpacaSetupPagePath[] = "/www/setup.html"; // Path to server and device setup page

const char kAlpacaJsonType[] = "application/json";
const uint32_t kAlpacaDiscoveryLength = 64;
const char kAlpacaDiscoveryHeader[] = "alpacadiscovery";

// Lambda Handler Function for calling object function
#define LHF(method) \
    (ArRequestHandlerFunction)[this](AsyncWebServerRequest * request) { this->method(request); }

class AlpacaDevice;

enum struct HttpStatus_t //
{
    kPassed = 200,         // request correctly formatted and passed to the device handler
    kInvalidRequest = 400, // device could not interprete the request
    kDeviceError = 500     // unexcpected device error
};

enum struct JsonValue_t
{
    kNoValue,
    kAsJsonStringValue,
    kAsPlainStringValue
};

enum struct Spelling_t
{
    kStrict = 0,
    kIgnoreCase = 1,
    kCheckBoth = 2,
    kNoMatch
};

struct AlpacaClient_t
{
    uint32_t client_id;             // connected with ClientID 1,... or 0 - not connected
    uint32_t client_transaction_id; // transactionId
    uint32_t time_ms;               // last client transaction time
    uint32_t max_service_time_ms;   // max time bitween two services
};

enum struct AlpacaErrorCode_t : int32_t
{
    Ok = 0,
    ActionNotImplementedException = (int32_t)0x0000040C, // to indicate that the requested action is not implemented in this driver.
    DriverBase = (int32_t)0x00000500,                    // The starting value for driver-specific error numbers.
    DriverMax = (int32_t)0x00000FFF,                     // The maximum value for driver-specific error numbers.
    InvalidOperationException = (int32_t)0x0000040B,     // to indicate that the requested operation can not be undertaken at this time.
    InvalidValue = (int32_t)0x00000401,                  // for reporting an invalid value.
    InvalidWhileParked = (int32_t)0x00000408,            // used to indicate that the attempted operation is invalid because the mount is currently in a Parked state.
    InvalidWhileSlaved = (int32_t)0x00000409,            // used to indicate that the attempted operation is invalid because the mount is currently in a Slaved state.
    NotConnected = (int32_t)0x00000407,                  // used to indicate that the communications channel is not connected.
    NotImplemented = (int32_t)0x00000400,                // for property or method not implemented.
    NotInCacheException = (int)0x0000040D,               // to indicate that the requested item is not present in the ASCOM cache.
    OperationCancelled = (int)0x0000040E,                // an (asynchronous) in-progress operation has been cancelled
    SettingsProviderError = (int)0x0000040A,             // related to settings.
    UnspecifiedError = (int)0x000004FF,                  // used when nothing else was specified.
    ValueNotSet = (int)0x00000402                        // for reporting that a value has not been set.
};

struct AlpacaRspStatus_t
{
    AlpacaErrorCode_t error_code;
    char error_msg[128];
    HttpStatus_t http_status;
};

class AlpacaServer
{
private:
    // Data for alpaca management description request
    String _mng_server_name = "empty";
    String _mng_manufacture = "DIY by @BigPet";
    String _mng_manufacture_version = "V1.0";
    String _mng_location = "Germany/Bavaria";

    // Logging see SLog
    String _syslog_host = "0.0.0.0";
    uint16_t _log_level = SLOG_DEBUG;
    bool _serial_log = true; // false/true: disable/enable logging after boot

    AsyncWebServer *_server_tcp;
    AsyncUDP _server_udp;
    uint16_t _port_tcp;
    uint16_t _port_udp;
    uint32_t _server_transaction_id = 0;

    char _uid[13] = {0}; // from wifi mac
    AlpacaDevice *_device[kAlpacaMaxDevices];
    int _n_devices = 0;

    bool _reset_request = false;

    AlpacaRspStatus_t _mng_rsp_status;
    AlpacaClient_t _mng_client_id;

    void _getApiVersions(AsyncWebServerRequest *request);
    void _getDescription(AsyncWebServerRequest *request);
    void _getConfiguredDevices(AsyncWebServerRequest *request);
    int32_t _paramIndex(AsyncWebServerRequest *request, const char *name, Spelling_t spelling);
    void _readJson(JsonObject &root);
    void _writeJson(JsonObject &root);
    void _getJsondata(AsyncWebServerRequest *request);
    void _getLinks(AsyncWebServerRequest *request);
    void _getSetupPage(AsyncWebServerRequest *request);

    void _respond(AsyncWebServerRequest *request, AlpacaClient_t &client, AlpacaRspStatus_t &rsp_status, const char *str, JsonValue_t jason_string_value);

public:
    AlpacaServer(const String mng_server_name,
                 const String mng_manufacture,
                 const String mng_manufacture_version,
                 const String mng_location);

    void Begin(uint16_t udp_port = kAlpacaUdpPort, uint16_t tcp_port = kAlpacaTcpPort, bool mount_little_fs = true);
    void RegisterCallbacks();
    void Loop();
    void AddDevice(AlpacaDevice *device);
    bool GetParam(AsyncWebServerRequest *request, const char *name, bool &value, Spelling_t spelling);
    bool GetParam(AsyncWebServerRequest *request, const char *name, float &value, Spelling_t spelling);
    bool GetParam(AsyncWebServerRequest *request, const char *name, double &value, Spelling_t spelling);
    bool GetParam(AsyncWebServerRequest *request, const char *name, int32_t &value, Spelling_t spelling);
    bool GetParam(AsyncWebServerRequest *request, const char *name, uint32_t &value, Spelling_t spelling);
    bool GetParam(AsyncWebServerRequest *request, const char *name, char *buffer, int buffer_size, Spelling_t spelling);

    void Respond(AsyncWebServerRequest *request, AlpacaClient_t &client, AlpacaRspStatus_t &rsp_status);
    void Respond(AsyncWebServerRequest *request, AlpacaClient_t &client, AlpacaRspStatus_t &rsp_status, int32_t int_value);
    void Respond(AsyncWebServerRequest *request, AlpacaClient_t &client, AlpacaRspStatus_t &rsp_status, double double_value);
    void Respond(AsyncWebServerRequest *request, AlpacaClient_t &client, AlpacaRspStatus_t &rsp_status, bool bool_value);
    void Respond(AsyncWebServerRequest *request, AlpacaClient_t &client, AlpacaRspStatus_t &rsp_status, const char *str_value, JsonValue_t jason_string_value);

    bool CheckMngClientData(AsyncWebServerRequest *req, Spelling_t spelling);

    void GetPath(AsyncWebServerRequest *request, const char *const path);
    bool LoadSettings();
    bool SaveSettings();
    void OnAlpacaDiscovery(AsyncUDPPacket &udpPacket);
    AsyncWebServer *getServerTCP() { return _server_tcp; }
    const char *GetUID() { return _uid; }
    const String GetSyslogHost() { return _syslog_host; };
    const uint16_t GetLogLvl() { return _log_level; };
    const bool GetSerialLog() { return _serial_log; };
    const bool GetResetRequest() { return _reset_request; };
    void SetResetRequest() { _reset_request = true; };

    // only for testing
    void RemoveSettingsFile() { LittleFS.remove(kAlpacaSettingsPath); }

    // Alpaca response status helpers ==============================================================================================
    void RspStatusClear(AlpacaRspStatus_t &rsp_status)
    {
        rsp_status.error_code = AlpacaErrorCode_t::Ok;
        rsp_status.http_status = HttpStatus_t::kPassed;
        strcpy(rsp_status.error_msg, "");
    }

// Alpaca Error responses
#define MYTHROW_RspStatusClientIDNotFound(req, rsp_status)                                                                   \
    {                                                                                                                        \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                             \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                      \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - '%s' not found", req->url().c_str(), "ClientID"); \
        goto mycatch;                                                                                                        \
    }

#define MYTHROW_RspStatusClientIDInvalid(req, rsp_status, clientID)                                                                     \
    {                                                                                                                                   \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                        \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                 \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - '%s=%d' invalid", req->url().c_str(), "ClientID", clientID); \
        goto mycatch;                                                                                                                   \
    }

#define MYTHROW_RspStatusClientTransactionIDNotFound(req, rsp_status)                                                                   \
    {                                                                                                                                   \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                        \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                 \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - '%s' not found", req->url().c_str(), "ClientTransactionID"); \
        goto mycatch;                                                                                                                   \
    }

#define MYTHROW_RspStatusClientTransactionIDInvalid(req, rsp_status, clientTransactionID)                                                                     \
    {                                                                                                                                                         \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                                              \
        rsp_status.http_status = HttpStatus_t::kInvalidRequest;                                                                                               \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - '%s=%d' invalid", req->url().c_str(), "ClientTransactionID", clientTransactionID); \
        goto mycatch;                                                                                                                                         \
    }

#define MYTHROW_RspStatusParameterNotFound(req, rsp_status, paraName)                                                                \
    {                                                                                                                                \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                     \
        rsp_status.http_status = HttpStatus_t::kInvalidRequest;                                                                      \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Parameter '%s' not found", req->url().c_str(), paraName); \
        goto mycatch;                                                                                                                \
    }

#define MYTHROW_RspStatusParameterInvalidInt32Value(req, rsp_status, paraName, paraValue)                                                       \
    {                                                                                                                                           \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                                \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                         \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Parameter '%s=%d invalid", req->url().c_str(), paraName, paraValue); \
        goto mycatch;                                                                                                                           \
    }

#define MYTHROW_RspStatusParameterInvalidBoolValue(req, rsp_status, paraName, paraValue)                                                                           \
    {                                                                                                                                                              \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                                                   \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                                            \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Parameter '%s=%s invalid", req->url().c_str(), paraName, paraValue ? "true" : "false"); \
        goto mycatch;                                                                                                                                              \
    }

#define MYTHROW_RspStatusParameterInvalidDoubleValue(req, rsp_status, paraName, paraValue)                                                      \
    {                                                                                                                                           \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                                \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                         \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Parameter '%s=%f invalid", req->url().c_str(), paraName, paraValue); \
        goto mycatch;                                                                                                                           \
    }

#define MYTHROW_RspStatusCommandStringInvalid(req, rsp_status, command_str)                                                              \
    {                                                                                                                                    \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;                                                                         \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                  \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Command string %s invalid", req->url().c_str(), command_str); \
        goto mycatch;                                                                                                                    \
    }

#define MYTHROW_RspStatusClientAlreadyConnected(req, rsp_status, clientID)                                                                              \
    {                                                                                                                                                   \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidOperationException;                                                                           \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                                 \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Client with 'ClientID=%d' already connected", req->url().c_str(), clientID); \
        goto mycatch;                                                                                                                                   \
    }

#define MYTHROW_RspStatusToMannyClients(req, rsp_status, maxNumOfClients)                                                                          \
    {                                                                                                                                              \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidOperationException;                                                                      \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                            \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Too many (%d) clients connected", req->url().c_str(), maxNumOfClients); \
        goto mycatch;                                                                                                                              \
    }

#define MYTHROW_RspStatusClientNotConnected(req, rsp_status, clientID)                                                                         \
    {                                                                                                                                          \
        rsp_status.error_code = AlpacaErrorCode_t::InvalidOperationException;                                                                  \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                        \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Client 'ClientID=%d' not connected", req->url().c_str(), clientID); \
        goto mycatch;                                                                                                                          \
    }

#define MYTHROW_RspStatusCommandNotImplemented(req, rsp_status, command)                                                                \
    {                                                                                                                                   \
        rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;                                                                      \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                 \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Command '%s' not implemented", req->url().c_str(), command); \
        goto mycatch;                                                                                                                   \
    }

#define MYTHROW_RspStatusActionNotImplemented(req, rsp_status, action, parameters)                                                                                     \
    {                                                                                                                                                                  \
        rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;                                                                                                     \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                                                                \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Action '%s' with Parameters '%s' not implemented", req->url().c_str(), action, parameters); \
        goto mycatch;                                                                                                                                                  \
    }

#define MYTHROW_RspStatusDeviceNotImplemented(req, rsp_status, device)                                                                \
    {                                                                                                                                 \
        rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;                                                                    \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                               \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Device '%s' not implemented", req->url().c_str(), device); \
        goto mycatch;                                                                                                                 \
    }

#define MYTHROW_RspStatusOperationCancelled(req, rsp_status, device)                                                                  \
    {                                                                                                                                 \
        rsp_status.error_code = AlpacaErrorCode_t::OperationCancelled;                                                                    \
        rsp_status.http_status = HttpStatus_t::kPassed;                                                                               \
        snprintf(rsp_status.error_msg, sizeof(rsp_status.error_msg), "%s - Device '%s' asynchronuous operation has been cancelled", req->url().c_str(), device); \
        goto mycatch;                                                                                                                 \
    }
};
