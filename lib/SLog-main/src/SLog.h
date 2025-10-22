/**************************************************************************************************
  Filename:       SLog.h
  Revised:        $Date: 2024-06-27$
  Revision:       $Revision: 01 $

  Description:    Serial and/or network (syslog) logger

  Copyright 2024 Peter Neeser. All rights reserved.
**************************************************************************************************/
#pragma once
#include <Arduino.h>
#include <WiFi.h>

/*
Usage:
    Init Serial: Begin()
    Init Network: Begin()

    Print: SLOG_PRINTF(lvl, format, ...)
*/

// syslog priorities (lvl)
#define SLOG_EMERGENCY 0
#define SLOG_ALERT 1
#define SLOG_CRITICAL 2
#define SLOG_ERROR 3
#define SLOG_WARNING 4
#define SLOG_NOTICE 5
#define SLOG_INFO 6
#define SLOG_DEBUG 7

// not a priority!!!
#define SLOG_UNKNOWN 8

// reduce code at compile time
#define SLOG_COMPILE_LVL SLOG_DEBUG

// common SLOG usage with printf
#define SLOG_PRINTF(lvl, msg, ...)                                                                                   \
    {                                                                                                                \
        if (lvl <= g_Slog.GetLvlMsk())                                                                               \
        {                                                                                                            \
            g_Slog._msg_line = g_Slog._line++;                                                                       \
            g_Slog._msg_lvl = lvl;                                                                                   \
            snprintf(g_Slog._msg_f, sizeof(g_Slog._msg_f), "%s", __FUNCTION__);                                      \
            snprintf(g_Slog._msg_msg, sizeof(g_Slog._msg_msg), msg, ##__VA_ARGS__);                                  \
            snprintf(g_Slog._msg_pf_msg, sizeof(g_Slog._msg_pf_msg), "%s:   %s", __PRETTY_FUNCTION__, g_Slog._msg_msg); \
            g_Slog.write();                                                                                          \
        }                                                                                                            \
    }

// Level specific SLOG printf usage - can be disabled at compile time to reduce code
#if SLOG_COMPILE_LVL >= SLOG_DEBUG
#define SLOG_DEBUG_PRINTF(msg, ...) SLOG_PRINTF(SLOG_DEBUG, msg, ##__VA_ARGS__);
#else
#define SLOG_DEBUG_PRINTF(msg, ...)
#endif

#if SLOG_COMPILE_LVL >= SLOG_INFO
#define SLOG_INFO_PRINTF(msg, ...) SLOG_PRINTF(SLOG_INFO, msg, ##__VA_ARGS__);
#else
#define SLOG_INFO_PRINTF(msg, ...)
#endif

#if SLOG_COMPILE_LVL >= SLOG_NOTICE
#define SLOG_NOTICE_PRINTF(msg, ...) SLOG_PRINTF(SLOG_NOTICE, msg, ##__VA_ARGS__);
#else
#define SLOG_NOTICE_PRINTF(msg, ...)
#endif

#define SLOG_WARNING_PRINTF(msg, ...) SLOG_PRINTF(SLOG_WARNING, msg, ##__VA_ARGS__);
#define SLOG_ERROR_PRINTF(msg, ...) SLOG_PRINTF(SLOG_ERROR, msg, ##__VA_ARGS__);
#define SLOG_CRITICAL_PRINTF(msg, ...) SLOG_PRINTF(SLOG_CRITICAL, msg, ##__VA_ARGS__);
#define SLOG_ALERT_PRINTF(msg, ...) SLOG_PRINTF(SLOG_ALERT, msg, ##__VA_ARGS__);
#define SLOG_EMERGENCY_PRINTF(msg, ...) SLOG_PRINTF(SLOG_EMERGENCY, msg, ##__VA_ARGS__);

const String _LVL_STR[] = {"EMERCENCY", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG", "UNKNOWN"};
extern class SLog g_Slog;

class SLog
{
#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
    USBCDC *_p_serial = nullptr;
#else // TODO to be tested
    HardwareSerial *_p_serial = nullptr;
#endif

    WiFiUDP *_p_syslog = nullptr;
    uint16_t _syslog_port = 514;
    IPAddress _syslog_address = IPAddress();

    bool _enable_serial = false;
    bool _enable_syslog = false;
    uint8_t _slog_lvl_msk = SLOG_DEBUG;

public:
    // variables to be used in SLOG_PRINTF macros have to be public
    uint32_t _line = 0;
    uint32_t _msg_line = 0;
    uint8_t _msg_lvl = SLOG_INFO;
    char _msg_f[33] = {0};
    char _msg_msg[1024] = {0};
    char _msg_pf_msg[1024] = {0};

    SLog(){};
    ~SLog()
    {
        if (_p_syslog)
            delete (_p_syslog);
        _p_syslog = nullptr;
    };

    // Init serial
#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
    void Begin(USBCDC &serial, int baudrate)
    {
        _p_serial = &serial;
        _p_serial->begin(baudrate);
        SetEnableSerial(true);
    };
#else // TODO test
    void Begin(HardwareSerial &serial, int baudrate)
    {
        _p_serial = &serial;
        _p_serial->begin(baudrate);
        SetEnableSerial(true);
    };
#endif

    // init network
    void Begin(const String host, uint16_t port = 514)
    {
        if (_p_syslog == nullptr)
        {
            _p_syslog = new WiFiUDP();
        }
        _syslog_address.fromString(host);
        SetEnableSyslog(true);
    };

    const bool SetEnableSerial(bool enable) { return _enable_serial = enable; };
    const bool SetEnableSyslog(bool enable) { return _enable_syslog = enable; };
    const bool GetEnableSerial() { return _enable_serial; };
    const bool GetEnableSyslog() { return _enable_syslog; };

    const uint8_t SetLvlMsk(uint8_t lvl) { return _slog_lvl_msk = lvl >= SLOG_WARNING && lvl <= SLOG_DEBUG ? lvl : SLOG_DEBUG; };
    const uint8_t GetLvlMsk() { return _slog_lvl_msk; };
    const String GetLvlMskStr() { return Lvl2Str(_slog_lvl_msk); };

    const String Lvl2Str(uint8_t lvl) { return _LVL_STR[lvl >= SLOG_EMERGENCY && lvl <= SLOG_DEBUG ? lvl : SLOG_UNKNOWN]; };

    void write()
    {
        if (_enable_serial)
        { // Serial format
            // line [lvl as string] __PRETTY_FUNCTION message
            _p_serial->printf("%4d [%9s] ", _msg_line, g_Slog.Lvl2Str(_msg_lvl));
            _p_serial->write(_msg_pf_msg, strlen(_msg_pf_msg));
        }
        if (_enable_syslog)
        { // SYSLOG format
            // <level>line __FUNCTION__ __PRETTY_FUNCTION__ message
            // Host: line, Facility: "kern", Priority: level, Tag: __FUNCTION__, Message: message
            _p_syslog->beginPacket(_syslog_address, _syslog_port);
            _p_syslog->printf("<%d>%d %s: ", _msg_lvl, _msg_line, _msg_f);
            _p_syslog->write((const uint8_t *)_msg_pf_msg, strlen(_msg_pf_msg));
            _p_syslog->endPacket();
        }
    }
};
