/**************************************************************************************************
  Filename:       Switch.cpp
  Revised:        $Date: 2025-10-21$
  Version:        Version: 2.2.0
  Description:    ASCOM Alpaca Switch Device for Kasa Smart Plugs

  Jessie Gentry   Devloped by myself for use with all my current Kasa Smart Plugs. Modified to handle child devices. Adaapted to
                  work with Alpaca. Able to scan through setup page all Kasa devices on the network. Stores enabled devices in
                  persistent storage.  Thanks to the following for their contributions to this project:
  KRIS JEARAKUL   The code ESP32AlpacaKasaSwitch for the Kasa smart switch is based on the  originally developed by Kris Jearakul.
                  This version has been adapted to fit into the ESP32AlpacaDevices framework by BigPet.

**************************************************************************************************/
#define VERSION "Version: 2.1.1"

// commend/uncommend to enable/disable device testsing with templates
#define TEST_COVER_CALIBRATOR     // create CoverCalibrator device
#define TEST_SWITCH               // create Switch device
#define TEST_OBSERVING_CONDITIONS // create ObservingConditions device
#define TEST_FOCUSER              // create Focuser device

// #define TEST_RESTART              // only for testing

#include <config.h>

#include <SLog.h>
#include <AlpacaDebug.h>
#include <AlpacaServer.h>
#include <Preferences.h>  // For clearing WiFi preferences
#include <esp_task_wdt.h> // For watchdog configuration


#ifdef TEST_SWITCH
#include <Switch.h>
Switch switchDevice;
#endif


// ASCOM Alpaca server with discovery
AlpacaServer alpaca_server(ALPACA_MNG_SERVER_NAME, ALPACA_MNG_MANUFACTURE, ALPACA_MNG_MANUFACTURE_VERSION, ALPACA_MNG_LOCATION);

#ifdef TEST_RESTART
// ========================================================================
// SW Restart
bool restart = false;                          // enable/disable
uint32_t g_restart_start_time_ms = 0xFFFFFFFF; // Timer for countdown
uint32_t const k_RESTART_DELAY_MS = 10000;     // Restart Delay

/**
 * SetRestart
 */
void ActivateRestart()
{
  restart = true;
  g_restart_start_time_ms = millis();
}

/*
 */
void checkForRestart()
{
  if (alpaca_server.GetResetRequest() || restart)
  {
    uint32_t timer_ms = millis() - g_restart_start_time_ms;
    uint32_t coun_down_sec = (k_RESTART_DELAY_MS - timer_ms) / 1000;

    if (timer_ms >= k_RESTART_DELAY_MS)
    {
      ESP.restart();
    }
  }
  else
  {
    g_restart_start_time_ms = millis();
  }
}
#endif

void setup()
{
  // Configure task watchdog timeout to 30 seconds
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  
  // setup logging and WiFi
  g_Slog.Begin(Serial, 115200);
#ifdef LOLIN_S2_MINI  
  delay(5000); // time to detect USB device
#endif  

  SLOG_INFO_PRINTF("ESP32ALPACAKasaSwitch ...\n");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);


  while (WiFi.status() != WL_CONNECTED)
  {
    SLOG_INFO_PRINTF("Connecting to WiFi ..\n");
    delay(1000);
  }
  {
    IPAddress ip = WiFi.localIP();
    char wifi_ipstr[32] = "xxx.yyy.zzz.www";
    snprintf(wifi_ipstr, sizeof(wifi_ipstr), "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
    SLOG_INFO_PRINTF("connected with %s\n", wifi_ipstr);
  }



  // setup ESP32AlpacaDevices
  // 1. Init AlpacaServer
  // 2. Init and add devices
  // 3. Finalize AlpacaServer
  alpaca_server.Begin();


#ifdef TEST_SWITCH
  switchDevice.Begin();
  alpaca_server.AddDevice(&switchDevice);
#endif


  alpaca_server.RegisterCallbacks();
  alpaca_server.LoadSettings();

  // finalize logging setup
  g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
  SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
  g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
  g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());

}

void loop()
{
  // Reset watchdog to prevent loopTask timeout
  esp_task_wdt_reset();
  
#ifdef TEST_RESTART
  checkForRestart();
#endif

  alpaca_server.Loop();

#ifdef TEST_SWITCH
  switchDevice.Loop();
  delay(10);
#endif

  // Yield to allow other tasks to run
  yield();
  delay(10);
}
