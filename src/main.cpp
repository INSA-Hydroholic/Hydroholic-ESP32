#include <Arduino.h>
#include "LoadCell.h"
#include "BLEManager.h"
#include "BatteryManager.h"
#include "Storage.h"

#define HX711_DOUT_PIN      17
#define HX711_SCK_PIN       16
#define LED_PIN             19
#define BATTERY_ADC_PIN     34
#define BUZZER_PIN          23
#define RST_BUTTON_PIN      18

BLEManager connection("Hydroholic");
Storage dataStorage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 2280.0);
BatteryManager batteryManager(BATTERY_ADC_PIN);

volatile bool isStorageReady = false;

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
        } else {
            Serial.println("No calibration factor found in config.csv, using default.");
        }
    } else {
        Serial.println("Could not open config.csv for reading.");
    }

    connection.begin(&dataStorage, &loadCell, &batteryManager);
    // Run the sensor task on core 1 so it can't starve the core-0 Idle/Watchdog
    xTaskCreatePinnedToCore(TaskLoadCell, "TaskLoadCell", 10000, &loadCell, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskBatteryManager, "TaskBatteryManager", 10000, &batteryManager, 1, NULL, 1);

    // TODO : check the mode of operation (wifi vs BLE) and start the appropriate task for the main loop (for now we just start the BLE task)
    ble_task_parameters_t* bleParams = new ble_task_parameters_t{&connection, &dataStorage};
    xTaskCreatePinnedToCore(TaskBLEManager, "TaskBLEManager", 10000, bleParams, 1, NULL, 0);
}

// Main loop will handle the state machine of the device, orchestrating the setup, mode of operation and other periodic tasks if needed
void loop() {
    Serial.println("Main loop heartbeat...");
    delay(5000);
}