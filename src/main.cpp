#include <Arduino.h>
#include "LoadCell.h"
#include "BLEManager.h"
#include "WiFiManager.h"
#include "BatteryManager.h"
#include "Storage.h"

#define UPDATE_INTERVAL 10000 // Interval for sending data to the server in milliseconds

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

volatile bool isStorageReady = false;

enum STATE {
    SETUP = 0,
    WIFI,
    BLE
};

STATE currentState = SETUP;

void setup() {
    Serial.begin(115200);

    if (dataStorage.begin()) {
        isStorageReady = true;
    } else {
        Serial.println("ERREUR : LittleFS HS");
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
                Serial.println("ERREUR : Impossible de créer config.csv");
            }
        }
    }

    pinMode(2, OUTPUT); // Builtin LED for status indication
    loadCell.begin();
    batteryManager.begin();

    // Search for existing calibration factor in config.csv
    File configFile = LittleFS.open("/config.csv", "r");
    if (configFile) {
        String line = configFile.readStringUntil('\n');
        configFile.close();
        if (line.startsWith("CALIB_FACTOR:")) {
            String factorStr = line.substring(line.indexOf(':') + 1);
            float factor = factorStr.toFloat();
            if (factor > 0) {
                loadCell.setCalibrationFactor(factor);
                Serial.print("Calibration factor loaded from config: ");
                Serial.println(factor, 6);
            } else {
                Serial.println("Invalid calibration factor in config.csv, using default.");
            }
        } else if (line.startsWith("DEV_ID:")) {
            String devId = line.substring(line.indexOf(':') + 1);
            Serial.print("Device ID loaded from config: ");
            Serial.println(devId);
            // TODO : We could store this device ID in a global variable or pass it to the WiFiManager for use in API calls
        } else if (line.startsWith("OP_MODE:")) {
            String opMode = line.substring(line.indexOf(':') + 1);
            Serial.print("Operation mode loaded from config: ");
            Serial.println(opMode);
            currentState = (opMode == "WIFI") ? WIFI : BLE; // Set initial state based on config
        } else {
            Serial.println("No calibration factor found in config.csv, using default.");
        }
    } else {
        Serial.println("Could not open config.csv for reading.");
    }

    // Run the sensor task on core 1 so it can't starve the core-0 Idle/Watchdog
    xTaskCreatePinnedToCore(TaskLoadCell, "TaskLoadCell", 10000, &loadCell, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskBatteryManager, "TaskBatteryManager", 10000, &batteryManager, 1, NULL, 1);

    if (currentState == WIFI) {
        wifiManager = new WiFiManager();
        wifiManager->begin("Joule", "senha123", opmode::NORMAL);
    } else if (currentState == SETUP) {
        wifiManager = new WiFiManager();
        wifiManager->begin("", "", opmode::CONFIGURATION);
    } else if (currentState == BLE) {
        bleManager = new BLEManager("Hydroholic");
        // Create a structure to hold the parameters for the BLE task, since we can only pass a single void* parameter to the task function
        ble_task_parameters_t* bleParams = new ble_task_parameters_t{&bleManager, &dataStorage, &loadCell, &batteryManager};
        xTaskCreatePinnedToCore(TaskBLEManager, "TaskBLEManager", 10000, bleParams, 1, NULL, 0);
    }
}

// Main loop will handle orchestration of services. It will handle weight updates, time synchronization, data storage, and others.
void loop() {
    if (currentState == BLE) {
        // The BLEManager task handles everything, so we just run an infinite loop
        delay(1000);
        return;
    } else if (currentState == WIFI) {
        // Send data to the server every UPDATE_INTERVAL seconds
        static unsigned long lastUpdateTime = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
            lastUpdateTime = currentTime;

            // Read the latest weight and battery level
            float weight = loadCell.getWeight();
            float batteryLevel = batteryManager.getBatteryLevel();

            // Create a JSON payload to send to the server
            String payload = "{\"weight\":" + String(weight, 2) + ",\"battery\":" + String(batteryLevel, 2) + "}";
            Serial.println("Sending data to server: " + payload);
            int responseCode = wifiManager->sendData("device", payload);
            if (responseCode > 0) {
                Serial.println("Data sent successfully with response code: " + String(responseCode));
            } else {
                Serial.println("Failed to send data to server");
            }
        }
    }

    delay(1000); // Main loop runs every 1 second
}