#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "Storage.h"

// PIN Definitions
#define HX711_DOUT_PIN 4
#define HX711_SCK_PIN 5

ConnectionManager connection("Hydroholic");
Storage stockage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 2280.0);
bool syncDone = false;

void TaskCapteur(void * pvParameters) {
    unsigned long lastSaveTime = 0;

    for(;;) {
        loadCell.measureWeight();
        float currentWeight = loadCell.getWeight();
        
        Serial.print("Poids mesuré (Core 0) : ");
        Serial.println(currentWeight);

        // Stockage local
        if (millis() - lastSaveTime > 60000) { 
            time_t now;
            time(&now);
            stockage.append((uint32_t)now, loadCell.getWeight());
            lastSaveTime = millis();
        }
        vTaskDelay(500 / portTICK_PERIOD_MS); 
    }
}

void setup() {
    Serial.begin(115200);

    if (!stockage.begin()) {
        Serial.println("ERREUR : Impossible d'initialiser LittleFS");
    }

    loadCell.begin();
    connection.begin(); 

    xTaskCreatePinnedToCore(TaskCapteur, "TaskCapteur", 10000, NULL, 1, NULL, 0);

    Serial.println("Système Hydroholic prêt !");
}

void loop() {
    if (connection.isConnected()) {
        if (!syncDone) {
            // we rename the file to sync.csv to indicate that it's being sent and to avoid conflicts if we want to keep recording new data while sending the history
            if (LittleFS.exists("/data.csv")) {
                LittleFS.rename("/data.csv", "/sync.csv");
                Serial.println("Fichier renommé en sync.csv pour envoi.");
            }

            // we open the sync file and send it 
            File syncFile = LittleFS.open("/sync.csv", "r");
            if (syncFile) {
                Serial.println("Envoi de l'historique ligne par ligne...");
                
                while (syncFile.available()) {
                    // we read line by line to avoid sending too much data at once and overwhelming the BLE connection
                    String line = syncFile.readStringUntil('\n');
                    
    
                    connection.sendHistoryChunk("HIST:" + line + "\n");
                    
                    // little delay to avoid overwhelming the BLE connection, especially if the history is long
                    delay(40); 
                }
                syncFile.close();
                syncDone = true;
                Serial.println("Fin de l'envoi. En attente du OK...");
            }
        }

        // if we received the OK signal from the client, we can clear the storage
        if (connection.shouldClearStorage) {
            LittleFS.remove("/sync.csv"); // we delete the sync file, just in case
            connection.shouldClearStorage = false;
            Serial.println("Historique synchronisé et supprimé !");
        }

        // send current weight for real-time display
        connection.updateWeight(loadCell.getWeight());

    } else {
        syncDone = false;
    }
    delay(1000);
}