#include <Arduino.h>
#include "LoadCell.h"
#include "BLEManager.h"
#include "WiFiManager.h"
#include "BatteryManager.h"
#include "Storage.h"
#include <RTCLib.h>
#include <Wire.h>  // For I2C connections
#include <time.h>

#define UPDATE_LOGS_INTERVAL 20000 // Interval for sending logs to the server in milliseconds
#define UPDATE_BATTERY_INTERVAL 10000 // Interval for updating battery status in milliseconds

#define HX711_DOUT_PIN      17
#define HX711_SCK_PIN       16
#define LED_PIN             19
#define BATTERY_ADC_PIN     34
#define BUZZER_PIN          23
#define RST_BUTTON_PIN      18

BLEManager* bleManager;
WiFiManager* wifiManager;
Storage dataStorage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 2280.0);
BatteryManager batteryManager(BATTERY_ADC_PIN);
RTC_DS1307 rtc;

volatile bool isStorageReady = false;
volatile bool isTimeSynched = false;
static unsigned long lastUpdateServerTime = 0;
static unsigned long lastSaveDataTime = 0;

enum STATE {
    SETUP = 0,
    WIFI,
    BLE
};

STATE currentState = WIFI; // Start in WiFi for debugging, MUST be changed to SETUP for production to allow initial configuration

void setup() {
    Serial.begin(115200);
    Wire.begin();  // TODO : check for power optimisations by turning off maybe
    if (!rtc.begin()) {
        Serial.println("ERROR : Couldn't communicate with the RTC DS1307");
    }
    rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));

    if (dataStorage.begin()) {
        isStorageReady = true;
    } else {
        Serial.println("ERROR : Couldn't start LittleFS data storage");
        // TODO : verify if it should continue or halt here
    }

    // Ensure required files exist before starting tasks to avoid creation races
    if (isStorageReady) {
        if (!LittleFS.exists("/data.csv")) {
            File fd = LittleFS.open("/data.csv", "a");
            if (fd) fd.close();
        }
        if (!LittleFS.exists("/temp.csv")) {
            File ft = LittleFS.open("/temp.csv", "a");
            if (ft) ft.close();
        }
        if (!LittleFS.exists("/config.csv")) {  // Create an empty config file if it doesn't exist
            File fs = LittleFS.open("/config.csv", "a");
            // Set default calibration factor in config.csv if it doesn't exist
            if (fs) {
                fs.println("CALIB_FACTOR:2280.0");
                fs.close();
            } else {
                Serial.println("ERROR : Failed to create config.csv");
            }
        }
    }

    pinMode(2, OUTPUT); // Builtin LED for status indication
    loadCell.begin();
    batteryManager.begin();

    String deviceID = "0";
    // Search for existing configuration in config.csv
    File configFile = LittleFS.open("/config.csv", "r");
    if (configFile) {
        String line = configFile.readStringUntil('\n');
        configFile.close();
        if (line.startsWith("CALIB_FACTOR:")) {     // CALIBRATION FACTOR
            String factorStr = line.substring(line.indexOf(':') + 1);
            float factor = factorStr.toFloat();
            if (factor > 0) {
                loadCell.setCalibrationFactor(factor);
                Serial.print("Calibration factor loaded from config: ");
                Serial.println(factor, 6);
            } else {
                Serial.println("Invalid calibration factor in config.csv, using default.");
            }
        } else if (line.startsWith("DEV_ID:")) {     // DEVICE ID
            deviceID = line.substring(line.indexOf(':') + 1);
            Serial.print("Device ID loaded from config: ");
            Serial.println(deviceID);
        } else if (line.startsWith("STATE:")) {     // OPERATION MODE
            String stateStr = line.substring(line.indexOf(':') + 1);
            if (stateStr == "WIFI") {
                currentState = WIFI;
            } else if (stateStr == "BLE") {
                currentState = BLE;
            } else {
                currentState = SETUP;
            }
            Serial.print("Operation mode loaded from config: ");
            Serial.println(currentState);
        } else {
            Serial.println("No valid configuration found in config.csv, using defaults.");
        }
    } else {
        Serial.println("Could not open config.csv for reading.");
    }

    // Run the sensor task on core 1 so it can't starve the core-0 Idle/Watchdog
    loadcell_task_parameters_t loadCellParams{&loadCell, &dataStorage, &rtc};
    xTaskCreatePinnedToCore(TaskLoadCell, "TaskLoadCell", 10000, &loadCellParams, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskBatteryManager, "TaskBatteryManager", 10000, &batteryManager, 1, NULL, 1);

    if (currentState == STATE::WIFI) {
        wifiManager = new WiFiManager(&rtc, deviceID);
        wifiManager->begin(WIFI_SSID, WIFI_PASS, opmode::NORMAL);  // Defined in environment.ini
    } else if (currentState == STATE::SETUP) {
        wifiManager = new WiFiManager(&rtc, deviceID);
        wifiManager->begin(NULL, NULL, opmode::CONFIGURATION);
    } else if (currentState == STATE::BLE) {
        bleManager = new BLEManager("Hydroholic");
        // Create a structure to hold the parameters for the BLE task, since we can only pass a single void* parameter to the task function
        ble_task_parameters_t bleParams{bleManager, &dataStorage, &loadCell, &batteryManager};
        xTaskCreatePinnedToCore(TaskBLEManager, "TaskBLEManager", 10000, &bleParams, 1, NULL, 0);
    }
}

// Main loop will handle orchestration of services. It will handle weight updates, time synchronization, data storage, and others.
void loop() {
    if (currentState == STATE::BLE) {
        // The BLEManager task handles everything, so we just run an infinite loop
        delay(1000);
        return;
    } else if (currentState == STATE::WIFI) {
        unsigned long currentTime = millis();
        // Send data to the server every UPDATE_INTERVAL seconds
        if (currentTime - lastUpdateServerTime >= UPDATE_LOGS_INTERVAL) {
            lastUpdateServerTime = currentTime;
            
            // Get current RTC time for timestamp synchronization with the server
            DateTime now = rtc.now();
            uint32_t epochTime = now.unixtime();
            Serial.print("Current RTC time: ");
            Serial.println(now.timestamp());
            // TODO : retrieve NTP time and compare with RTC time to check if it's synched, if not, sync it and update stored timestamps with the new time. For now, we will assume it's synched

            // Send loadcell logs to the server
            String hydration_logs = dataStorage.readContent(LOADCELL_DATA_FILE);  // TODO : optimize this by reading in chunks instead of the whole file at once
            if (hydration_logs.length() > 0) {
                int responseCode = wifiManager->sendData("/device/logs", hydration_logs, "text/csv");
                if (responseCode > 0) {
                    Serial.println("Data sent successfully with response code: " + String(responseCode));
                    dataStorage.clear(LOADCELL_DATA_FILE); // Clear the stored data after successful sync
                }
            }

            // Send battery status to the server
            float batteryLevel = batteryManager.getBatteryLevel();
            String payload = "{\"epoch\":" + String(epochTime) + ", \"battery\":" + String(batteryLevel, 2) + "}";
            Serial.println("Sending data to server: " + payload);
            wifiManager->sendData("/device/status", payload, "application/json");
        }
    }

    delay(1000); // Main loop runs every 1 second
}