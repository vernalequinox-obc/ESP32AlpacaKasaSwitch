/**************************************************************************************************
  Filename:       AlpacaDevice.h
  Revised:        $Date: 2024-01-14$
  Revision:       $Revision: 01 $

  Description:    ASCOM Alpaca Device common functions

  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
            based on https://github.com/elenhinan/ESPAscomAlpacaServer
**************************************************************************************************/
#pragma once
#include "AlpacaServer.h"

class AlpacaDevice
{
protected:
    // pointer to server
    AlpacaServer *_alpaca_server;
    // Data defined and requested by Alpaca
    char _device_type[30] = "empty";       // device type
    int32_t _device_interface_version = 0; // device type specific interface version
    char _device_uid[65] = "";             // unique device id
    char _device_description[129] = "";    // device description - set by config; init with default from specific device
    char _device_url[129] = "";            // /api/v1/<deviceType>/<deviceNumber>/setup

    int8_t _device_number = -1;            // A0,... for each device_type
    char _device_name[33] = "";            // device name - set by config; init with <deviceType>-<deviceNumber>

    char _device_and_driver_version[32] = "";
    char _driver_info[64] = "";

    char _supported_actions[512] = "[]";
    char _device_states[1024] = "[]";
    AlpacaClient_t _clients[kAlpacaMaxClients + 1]; // manage clients; [0] - unconnected client; [1,...] connected client
    AlpacaRspStatus_t _rsp_status;

    uint32_t _service_counter = 0;

    // bool _isconnected = false;

    void Begin();
    // common functions
    virtual void _setSetupPage();
    void _getJsondata(AsyncWebServerRequest *request);
    void _putJsondata(AsyncWebServerRequest *request);
    void createCallBack(ArRequestHandlerFunction fn, WebRequestMethodComposite type, const char command[]);
    void createCallBackUrl(ArRequestHandlerFunction fn, WebRequestMethodComposite type, const char url[], const char handler_name[]);
    void _getSetupPage(AsyncWebServerRequest *request);
    void _addAction(const char *const action);

    // alpaca commands

    // overload this functions in device specific class if implemended
    virtual void AlpacaPutAction(AsyncWebServerRequest *request);
    virtual void AlpacaPutCommandBlind(AsyncWebServerRequest *request);
    virtual void AlpacaPutCommandBool(AsyncWebServerRequest *request);
    virtual void AlpacaPutCommandString(AsyncWebServerRequest *request);

    virtual void AlpacaPutConnected(AsyncWebServerRequest *request);
    virtual void AlpacaPutConnect(AsyncWebServerRequest *request);
    virtual void AlpacaPutDisconnect(AsyncWebServerRequest *request);   
    
    virtual void AlpacaGetConnected(AsyncWebServerRequest *request);
    virtual void AlpacaGetConnecting(AsyncWebServerRequest *request);

    void AlpacaGetDescription(AsyncWebServerRequest *request);
    void AlpacaGetDriverInfo(AsyncWebServerRequest *request);
    void AlpacaGetDriverVersion(AsyncWebServerRequest *request);
    void AlpacaGetInterfaceVersion(AsyncWebServerRequest *request);
    void AlpacaGetName(AsyncWebServerRequest *request);
    void AlpacaGetSupportedActions(AsyncWebServerRequest *request);
    void AlpacaGetDeviceState(AsyncWebServerRequest *request);

    virtual const bool _getDeviceStateList(size_t buf_len, char* buf) = 0;
    // helpers
    int32_t checkClientDataAndConnection(AsyncWebServerRequest *request, uint32_t &clientIdx, Spelling_t spelling);
    uint32_t getClientIdxByClientID(uint32_t clientID);

public:
    void virtual RegisterCallbacks();
    void SetAlpacaServer(AlpacaServer *alpaca_server) { _alpaca_server = alpaca_server; }
    void SetDeviceNumber(int8_t device_number);
    void CheckClientConnectionTimeout();
    const uint8_t GetDeviceNumber() { return _device_number; }
    const char *GetDeviceType() { return _device_type; }
    const char *GetDeviceName() { return _device_name; };
    const char *GetDeviceUID() { return _device_uid; }
    const char *GetDeviceURL() { return _device_url; };
    virtual void AlpacaReadJson(JsonObject &root);
    virtual void AlpacaWriteJson(JsonObject &root);
    const uint32_t GetNumberOfConnectedClients();
    const uint32_t GetServiceCounter() { return _service_counter; };
};