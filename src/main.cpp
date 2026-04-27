#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "BatteryManager.h"
#include "Storage.h"

#define HX711_DOUT_PIN 14
#define HX711_SCK_PIN 27
#define HX711_ENABLE_PIN 26
#define BATTERY_ADC_PIN 34

ConnectionManager connection("Hydroholic");
Storage dataStorage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, HX711_ENABLE_PIN, 2280.0);
BatteryManager batteryManager(BATTERY_ADC_PIN);

volatile bool isTimeSynched = false;
volatile bool isSyncing = false;
volatile bool isWaitingForConfirm = false;
volatile bool isStorageReady = false;
volatile float globalWeight = 0.0;


void TaskBatteryManager(void * pvParameters) {
    for(;;) {
        batteryManager.readBatteryLevel();
        float level = batteryManager.getBatteryLevel();
        Serial.print("Niveau de batterie : ");
        Serial.println(level);
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Run every 500 ms
    }
}

void setup() {
    Serial.begin(115200);
    // On force le formatage si besoin avec true
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

    connection.begin(&dataStorage, &isTimeSynched, &loadCell, &batteryManager);
    // Run the sensor task on core 1 so it can't starve the core-0 Idle/Watchdog
    xTaskCreatePinnedToCore(TaskLoadCell, "TaskLoadCell", 10000, &loadCell, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskBatteryManager, "TaskBatteryManager", 10000, NULL, 1, NULL, 1);
}

void loop() {
    if (!connection.isConnected()) {
        isSyncing = false;
        isWaitingForConfirm = false;
        // Blink builtin LED to indicate waiting for connection
        digitalWrite(2, millis() / 500 % 2);
        delay(100);
        return;
    }
    digitalWrite(2, LOW); // Turn off LED when connected

    // STATE 2 : Attente de l'heure
    if (!isTimeSynched) {
        delay(1000);
        return;
    }

    // STATE 3 : Préparation (On n'entre ici que si on n'est pas déjà en train de synchro)
    if (isTimeSynched && !isSyncing && !isWaitingForConfirm) {
        if (dataStorage.prepareDataForSync()) {
            isSyncing = true;
            Serial.println("Début de l'envoi...");
        } else {
            // Pas de données à envoyer, on passe directement en mode "prêt"
            isWaitingForConfirm = true; 
        }
    }

    // STATE 4 : Envoi streaming
    if (isSyncing) {
        File f = LittleFS.open("/sync.csv", "r"); 
        if (f) {
            Serial.println("Envoi sécurisé de l'historique...");
            while (f.available()) {
                String line = f.readStringUntil('\n');
                if (line.length() > 0) {
                    connection.sendInformation("HIST:" + line + "\n");
                    delay(50); 
                }
            }
            f.close();
            isSyncing = false;
            isWaitingForConfirm = true; 
            Serial.println("Envoi fini. Attente du OK client...");
        } else {
            // SI LE FICHIER N'EXISTE PAS OU NE PEUT PAS S'OUVRIR
            Serial.println("Erreur : Impossible d'ouvrir sync.csv, abandon de la synchro.");
            isSyncing = false; 
        }
    }

    // STATE 5 : Réception du OK (Vider le stockage)
    if (connection.shouldClearStorage) {
        connection.shouldClearStorage = false; 
        if (LittleFS.exists("/sync.csv")) {
            dataStorage.clearSyncFile();
            Serial.println("Fichier de synchro supprimé !");
        }
        isWaitingForConfirm = false; 
    }

    // Poids en temps réel
   connection.updateWeight(globalWeight);
    delay(1000);
}