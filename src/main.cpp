#include <Arduino.h>
#include "LoadCell.h"
#include "BLEManager.h"
#include "WiFiManager.h"
#include "BatteryManager.h"
#include "HMIManager.h"
#include "Storage.h"
#include "constants.h"
#include <RTCLib.h>
#include <Wire.h>  // For I2C connections
#include <time.h>

#define HX711_DOUT_PIN      17
#define HX711_SCK_PIN       16
#define LED_PIN             19
#define BATTERY_ADC_PIN     34
#define BUZZER_PIN          23
#define RST_BUTTON_PIN      18

BLEManager* bleManager;
WiFiManager* wifiManager;
Storage dataStorage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 1000.0);
BatteryManager batteryManager(BATTERY_ADC_PIN);
RTC_DS1307 rtc;
HMIManager hmiManager(RST_BUTTON_PIN, LED_PIN, BUZZER_PIN);

volatile bool isStorageReady = false;
volatile bool isTimeSynched = false;
static unsigned long lastUpdateServerTime = 0;
static unsigned long lastGetReminderTime = 0;
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

    // Retrieve rtc time, if it's later than the compile time, it means the rtc has a valid time (either from previous sync or from battery backup) and we can use it. Otherwise, we set it to the compile time as a fallback
    if (rtc.now().unixtime() > DateTime(F(__DATE__),F(__TIME__)).unixtime()) {
        Serial.println("RTC time is valid, using it.");
    } else {
        Serial.println("RTC time is not valid, setting it to compile time.");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    if (dataStorage.begin()) {
        isStorageReady = true;
    } else {
        Serial.println("ERROR : Couldn't start LittleFS data storage");
        ESP.restart();  // Restart to attempt recovery from partitioning
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
                fs.println("CALIB_FACTOR:1000.0");
                fs.close();
            } else {
                Serial.println("ERROR : Failed to create config.csv");
            }
        }
    }

    pinMode(2, OUTPUT); // Builtin LED for status indication
    loadCell.begin();
    batteryManager.begin();
    hmiManager.begin();

    // Flash LEDs twice at startup to indicate the device has booted successfully and is starting the tasks
    hmiManager.startBlinkingLEDs();
    hmiManager.animateLEDs(500);
    delay(300);
    hmiManager.animateLEDs(500);
    delay(300);
    hmiManager.stopBlinkingLEDs();

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
    hmi_task_parameters_t hmiParams{&hmiManager};
    xTaskCreatePinnedToCore(TaskHMIManager, "TaskHMIManager", 10000, &hmiParams, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskBlinkLEDs, "TaskBlinkLEDs", 10000, &hmiParams, 1, NULL, 1);

    // Run WiFi and BLE tasks on core 0, since drivers already run there anyways.
    if (currentState == STATE::WIFI) {
        Serial.println("Running in WIFI mode.");
        wifiManager = new WiFiManager(&rtc, deviceID);
        wifiManager->begin(WIFI_SSID, WIFI_PASS, opmode::NORMAL);  // Defined in environment.ini
        xTaskCreatePinnedToCore(TaskWiFiManager, "TaskWiFiManager", 10000, wifiManager, 1, NULL, 0);
    } else if (currentState == STATE::SETUP) {
        Serial.println("Running in SETUP mode. Starting WiFi AP for configuration.");
        wifiManager = new WiFiManager(&rtc, deviceID);
        wifiManager->begin(NULL, NULL, opmode::CONFIGURATION);
        xTaskCreatePinnedToCore(TaskWiFiManager, "TaskWiFiManager", 10000, wifiManager, 1, NULL, 0);
    } else if (currentState == STATE::BLE) {
        Serial.println("Running in BLE mode.");
        bleManager = new BLEManager("Hydroholic");
        // Create a structure to hold the parameters for the BLE task, since we can only pass a single void* parameter to the task function
        ble_task_parameters_t bleParams{bleManager, &dataStorage, &loadCell, &batteryManager};
        xTaskCreatePinnedToCore(TaskBLEManager, "TaskBLEManager", 10000, &bleParams, 1, NULL, 0);
    }
}

// The main loop will handle the orchestration of services
void loop() {
    // RESET Request handling
    // TODO : clear storage and safely reset the microcontroller
    // Start blinking LEDs to confirm the button press has been registered and a reset request is being processed while waiting for the reset duration to be reached in the TaskHMIManager
    if (hmiManager.getResetRequest()) {
        hmiManager.setResetRequest(false); // Clear the reset request flag to avoid re-entering this block
        Serial.println("Reset requested...");
        hmiManager.setBlinkDuration(200); // Set a faster blink duration to indicate reset is being processed
        hmiManager.startBlinkingLEDs();
        dataStorage.clear(LOADCELL_DATA_FILE);
        // dataStorage.clear(CONFIG_DATA_FILE);
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait for 5 seconds to allow the user to see the indication before restarting
        // ESP.restart();
        Serial.println("Restarting device...");
    }

    if (currentState == STATE::BLE) {
        // The BLEManager task handles everything, so we just run an infinite loop
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    } else if (currentState == STATE::WIFI) {
        unsigned long currentTime = millis();
        // Check if user needs to be reminded to interact with the device every GET_REMINDER_INTERVAL seconds (e.g. to drink water, etc)
        if (currentTime - lastGetReminderTime >= GET_REMINDER_INTERVAL) {
            lastGetReminderTime = currentTime;
            Serial.println("Checking if user needs to be reminded...");
            // String response = "";
            // String &responseRef = response; // Need a non-const reference to pass to the getData function
            // if(wifiManager->getData("/device/:ID/user?"+wifiManager->getDeviceID(), responseRef, "text/plain")) {
            //     Serial.println("Received associated user ID from server: " + response);
            //     if (response != "-1") {
            //         String userID = response; // Store user ID for future use if needed
            //         response = ""; // Clear response string to reuse it for the next request
            //         String &responseRef = response; // Update reference to the cleared response string
            //         if (wifiManager->getData("/device/:ID/reminder?"+wifiManager->getDeviceID(), responseRef, "text/plain")) {
            //             Serial.println("Received reminder response from server: " + response);
            //             if (response == "1") {
            //                 hmiManager.remindUser();
            //             }
            //         } else {
            //             Serial.println("Error checking reminder status from server.");
            //         }
            //     }
            // } else {
            //     Serial.println("Error checking reminder status from server.");
            // }
            hmiManager.remindUser();
        }
        // Send data to the server every UPDATE_LOGS_INTERVAL seconds
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
                Serial.println("Sending " + String(hydration_logs.length()) + " bytes of hydration logs to server:\n" + hydration_logs);
                if(wifiManager->sendData("/device/:ID/logs", hydration_logs, "text/csv")) {
                    Serial.println("Data sent successfully.");
                    dataStorage.clear(LOADCELL_DATA_FILE); // Clear the stored data after successful sync
                } else {
                    Serial.println("Error sending logs to server.");
                    // TODO : handle failed data sync (retry mechanism, etc)
                }
            }

            // Send battery status to the server
            float batteryLevel = batteryManager.getBatteryLevel();
            String payload = "{\"epoch\":" + String(epochTime) + ", \"battery\":" + String(batteryLevel, 2) + "}";
            Serial.println("Sending data to server: " + payload);
            wifiManager->sendData("/device/:ID/status", payload, "application/json");
        }
        
        // Retrieve alert status from the server every ALERT_CHECK_INTERVAL seconds to check if we need to trigger an alert on the device
    }

    vTaskDelay(250 / portTICK_PERIOD_MS); // Main loop runs every 250 ms
}