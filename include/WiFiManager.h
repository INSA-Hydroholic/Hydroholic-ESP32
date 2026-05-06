#pragma once

#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <RTClib.h>
#include "constants.h"

void TaskWiFiManager(void * pvParameters);  // Expects a pointer to a WiFiManager instance as parameter

enum opmode {
    NORMAL,
    CONFIGURATION
};

struct Header {
    String name;
    String value;
};

// The WiFi will only be activated when trying to send data, and will be turned off to save battery when not in use. The WiFiManager will handle the connection and disconnection to the WiFi network, as well as the fallback to AP mode for configuration if the connection fails.
class WiFiManager {
    public:
        WiFiManager(RTC_DS1307* realTimeClock, String deviceID = "0");
        void begin(const char* ssid, const char* password, opmode mode = NORMAL);
        bool isConnected() const;
        bool getData(const String& endpoint, String& response, const String& contentType = "text/csv", const Header* headers = nullptr, uint8_t numHeaders = 0); // Returns true on success, false on failure
        bool sendData(const String& endpoint, const String& payload, const String& contentType = "text/csv"); // Returns true on success, false on failure
        bool connect(const char* ssid = nullptr, const char* password = nullptr, bool retry = true); // Connect to WiFi using provided credentials or stored ones
        bool disconnectAndDisable();
        opmode getMode() const { return _mode; }
        void setMode(opmode mode) { _mode = mode; }
        bool syncNTP();  // Synchronize time with NTP server
        String getCurrentTime() const { return rtc->now().timestamp(); } // Get current time from RTC as a string
        void setAPIURL(const String& url);
        String getDeviceID() const { return _deviceID; }

    private:
        opmode _mode;
        const char* _apssid = "Hydroholic-Setup";
        char* _ssid;
        char* _password;
        String _deviceID;
        String apiURL = API_URL; // Defined in environment.ini
        RTC_DS1307* rtc;
};