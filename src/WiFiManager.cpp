#include "WiFiManager.h"

void TaskWiFiManager(void * pvParameters) {
    WiFiManager* manager = static_cast<WiFiManager*>(pvParameters);
    for(;;) {
        if (!manager->isConnected()) {
            // Blink builtin LED to indicate waiting for WiFi connection
            digitalWrite(2, millis() / 500 % 2);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }
        digitalWrite(2, LOW); // Turn off LED when connected

        // Synchronize time with NTP server if not already synchronized
        manager->syncNTP();

        vTaskDelay(DELAY_10_MINS / portTICK_PERIOD_MS);
    }
}

WiFiManager::WiFiManager(RTC_DS1307* realTimeClock, String deviceID) : rtc(realTimeClock), _deviceID(deviceID) {}

void WiFiManager::begin(const char* ssid, const char* password, opmode mode) {
    setMode(mode);
    if (mode == opmode::CONFIGURATION) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(_apssid, NULL, 1, false, 1); // Open AP with channel 1 and max connections of 1 to avoid interference 
        Serial.println("WiFi AP started with SSID: " + String(_apssid));
        return;
    } else {
        WiFi.mode(WIFI_STA);
        if(!connect(ssid, password)) {
            Serial.println("\nFailed to connect to WiFi. Retrying...");
            for 
            this->begin("", "", opmode::CONFIGURATION); // Fallback to AP mode for configuration
            return;
        }
        _ssid = (char*)ssid;
        _password = (char*)password;
        Serial.println("\nWiFi connected with IP: " + WiFi.localIP().toString());
        // Retrieve device ID if not set, for use in identification when sending data
        if (_deviceID == "0") {
            _deviceID = WiFi.macAddress();
            // TODO : for now we'll be using the macAddress as device ID, but ideally it'd be server issued
        }
        // Register the device with the server using the device ID, so it can be identified when sending data
        int responseCode = sendData("/device/register", "{\"deviceID\":\"" + _deviceID + "\"}", "application/json");
        if (responseCode > 0) {
            Serial.println("Device registered successfully with response code: " + String(responseCode));
        } else {
            Serial.println("Error registering device: " + String(responseCode));
            // TODO : handle registration failure (retry, fallback to AP mode for configuration, etc)
        }
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

int WiFiManager::sendData(const String& endpoint, const String& payload, const String& contentType) {
    if(connect()) {
        Serial.println("WiFi connected, sending data...");
    } else {
        Serial.println("Error: Unable to connect to WiFi, cannot send data");
        return -1;
    }

    // Parse endpoint to replace :ID with actual device ID for dynamic endpoint generation
    String parsedEndpoint = endpoint;
    parsedEndpoint.replace(":ID", _deviceID);

    HTTPClient http;
    http.begin(String(apiURL) + parsedEndpoint);
    http.addHeader("Content-Type", contentType);

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
        Serial.println("Data sent successfully, response code: " + String(httpResponseCode));
    } else {
        Serial.println("Error sending data: " + String(http.errorToString(httpResponseCode)));
    }
    http.end();

    disconnectAndDisable(); // Disconnect WiFi to save power after sending data
    return httpResponseCode;
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    if (isConnected()) {
        return true;
    }
    if (ssid && password) {
        WiFi.begin(ssid, password);
    } else {
        WiFi.begin(_ssid, _password);
    }
    Serial.print("Connecting to WiFi");
    time_t startAttemptTime = millis();
    const unsigned long connectionTimeout = 30000; // 30 seconds timeout for WiFi
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < connectionTimeout) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }
    Serial.println("\nWiFi connected with IP: " + WiFi.localIP().toString());
    return true;
}


// Returns true if WiFi was disconnected, false if it was not connected or an error occurred
bool WiFiManager::disconnectAndDisable() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, false); // Disconnect but keep credentials for reconnection
        WiFi.mode(WIFI_OFF); // Turn off WiFi to save power
        Serial.println("WiFi disconnected and disabled");
        return true;
    } else {
        Serial.println("WiFi is not connected, nothing to disconnect");
        WiFi.mode(WIFI_OFF); // Ensure WiFi is off to save power
        return false;
    }
    return false;
}

bool WiFiManager::syncNTP() {
    if (!isConnected()) {
        Serial.println("Cannot synchronize time: WiFi not connected");
        return false;
    }
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Synchronizing time with NTP server...");
    time_t now = time(nullptr);
    int retry = 0;
    const int maxRetries = 10;
    while (now < 8 * 3600 * 2 && retry < maxRetries) { // Wait until time is set (after Jan 1, 1970)
        delay(1000);
        Serial.print(".");
        now = time(nullptr);
        retry++;
    }
    if (now < 8 * 3600 * 2) {
        Serial.println("\nFailed to synchronize time with NTP server");
        return false;
    }
    Serial.println("\nTime synchronized successfully: " + String(ctime(&now)));
    // Update RTC time
    rtc->adjust(DateTime(now));
    return true;
}

void WiFiManager::setAPIURL(const char* url) {
    apiURL = (char*)url;
    Serial.println("API URL set to: " + String(apiURL));
    // TODO : save the API URL in Storage config.csv file for persistence across reboots
}