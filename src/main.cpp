#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "Storage.h"

#define HX711_DOUT_PIN 4
#define HX711_SCK_PIN 5

ConnectionManager connection("Hydroholic");
Storage storage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 2280.0);

volatile bool isTimeSynched = false;
volatile bool isSyncing = false;
volatile bool isWaitingForConfirm = false; 
volatile bool isStorageReady = false;      
volatile float globalWeight = 0.0;

void TaskCapteur(void * pvParameters) {
    unsigned long lastSaveTime = 0;
    for(;;) {
        loadCell.measureWeight();
        globalWeight = loadCell.getWeight(); 
        
        
        if (isStorageReady && (millis() - lastSaveTime > 60000)) {
            lastSaveTime = millis(); 
            time_t now;
            time(&now);
            if (!storage.append((uint32_t)now, globalWeight, isTimeSynched)) {
                Serial.println("Archive reportée (FS occupé)");
            } else {
                Serial.println("Donnée archivée.");
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    // On force le formatage si besoin avec true
    if (storage.begin()) {
        isStorageReady = true;
    } else {
        Serial.println("ERREUR : LittleFS HS");
    }

    loadCell.begin();
    connection.begin(&storage, (bool*)&isTimeSynched);
    xTaskCreatePinnedToCore(TaskCapteur, "TaskCapteur", 10000, NULL, 1, NULL, 0);
}

void loop() {
    if (!connection.isConnected()) {
        isSyncing = false;
        isWaitingForConfirm = false;
        delay(100);
        return;
    }

    // STATE 2 : Attente de l'heure
    if (!isTimeSynched) {
        delay(1000);
        return;
    }

    // STATE 3 : Préparation (On n'entre ici que si on n'est pas déjà en train de synchro)
    if (isTimeSynched && !isSyncing && !isWaitingForConfirm) {
        if (storage.prepareDataForSync()) {
            isSyncing = true;
            Serial.println("Début de l'envoi...");
        } else {
            // Pas de données à envoyer, on passe directement en mode "prêt"
            isWaitingForConfirm = true; 
        }
    }

    // STATE 4 : Envoi streaming
    // Dans main.cpp - STATE 4
    if (isSyncing) {
        File f = LittleFS.open("/sync.csv", "r"); 
        if (f) {
            Serial.println("Envoi sécurisé de l'historique...");
            while (f.available()) {
                String line = f.readStringUntil('\n');
                if (line.length() > 0) {
                    connection.sendHistoryChunk("HIST:" + line + "\n");
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
            storage.clearSyncFile();
            Serial.println("Fichier de synchro supprimé !");
        }
        isWaitingForConfirm = false; 
    }

    // Poids en temps réel
   connection.updateWeight(globalWeight);
    delay(1000);
}