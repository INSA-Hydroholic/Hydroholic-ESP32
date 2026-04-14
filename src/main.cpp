#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "Storage.h"

#define HX711_DOUT_PIN 14
#define HX711_SCK_PIN 27
#define HX711_ENABLE_PIN 26

ConnectionManager connection("Hydroholic");
Storage storage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, HX711_ENABLE_PIN, 2280.0);

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
        Serial.print("Poids actuel : ");
        Serial.println(globalWeight);
        
        if (isStorageReady && (millis() - lastSaveTime > 60000)) {
            lastSaveTime = millis(); 
            time_t now;
            time(&now);
            Serial.print("Poids mesuré : ");
            Serial.print(globalWeight);
            Serial.print(" - Heure : ");
            Serial.println((uint32_t)now);

            if (!storage.append((uint32_t)now, globalWeight, isTimeSynched)) {
                Serial.println("Archive reportée (FS occupé)");
            } else {
                Serial.println("Donnée archivée.");
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Run every 500 ms
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
    }

    pinMode(2, OUTPUT); // Builtin LED for status indication
    loadCell.begin();
    connection.begin(&storage, (bool*)&isTimeSynched, &loadCell);
    // Run the sensor task on core 1 so it can't starve the core-0 Idle/Watchdog
    xTaskCreatePinnedToCore(TaskCapteur, "TaskCapteur", 10000, NULL, 1, NULL, 1);
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
        if (storage.prepareDataForSync()) {
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