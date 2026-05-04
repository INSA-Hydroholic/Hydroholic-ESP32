#pragma once

#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <RTClib.h>

#define DELAY_10_MINS 600000  // 10 minutes in milliseconds

void TaskWiFiManager(void * pvParameters);  // Expects a pointer to a WiFiManager instance as parameter

enum opmode {
    NORMAL,
    CONFIGURATION
};

// The WiFi will only be activated when trying to send data, and will be turned off to save battery when not in use. The WiFiManager will handle the connection and disconnection to the WiFi network, as well as the fallback to AP mode for configuration if the connection fails.
class WiFiManager {
    public:
        WiFiManager(RTC_DS1307* realTimeClock);
        void begin(const char* ssid, const char* password, opmode mode = NORMAL);
        bool isConnected() const;
        int sendData(const String& endpoint, const String& payload); // Returns HTTP status code or -1 on failure
        bool connect(const char* ssid = nullptr, const char* password = nullptr); // Connect to WiFi using provided credentials or stored ones
        bool disconnectAndDisable();
        opmode getMode() const { return _mode; }
        void setMode(opmode mode) { _mode = mode; }
        bool syncNTP();  // Synchronize time with NTP server

    private:
        opmode _mode;
        const char* _apssid = "Hydroholic-Setup";
        char* _ssid;
        char* _password;
        char* apiURL = "https://agir.minettoar.org/api/";
        RTC_DS1307* rtc;
};