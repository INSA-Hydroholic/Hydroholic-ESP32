#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "Storage.h"

// PIN Definitions
#define HX711_DOUT_PIN 4
#define HX711_SCK_PIN 5

// Global state
ConnectionManager connection("Hydroholic");
Storage storage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 2280.0);

// Bool assignments are atomic on ESP32 - no need for mutex
// Volatile prevents compiler from optimizing and caching the value
volatile bool isTimeSynched = false;  // Set by BLE characteristic onWrite callback
volatile bool isSyncing = false;      // Set when we start sending sync.csv to client


void TaskCapteur(void * pvParameters) {
    unsigned long lastSaveTime = 0;
    for(;;) {
        loadCell.measureWeight();
        float currentWeight = loadCell.getWeight();
        
        Serial.print("Measured weight (Core 0) : ");
        Serial.println(currentWeight);

        time_t now;
        time(&now);
        // Store with isTimeSynched - volatile read is sufficient for bool
        storage.append((uint32_t)now, currentWeight, isTimeSynched);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);

    if (!storage.begin()) {
        Serial.println("Error at LittleFS startup");
    }

    loadCell.begin();
    connection.begin(&storage, (bool*)&isTimeSynched);

    xTaskCreatePinnedToCore(TaskCapteur, "TaskCapteur", 10000, NULL, 1, NULL, 0);

    Serial.println("Hydroholic system ready");
}

void loop() {
    // For the loop, we define 4 main states based on connection and sync status:

    // State 1: Not connected - wait for connection
    if (!connection.isConnected()) {
        // Reset sync flag on disconnect
        if (isSyncing) {
            isSyncing = false;
            Serial.println("Disconnected. Sync cancelled.");
        }
        delay(100);
        return;
    }

    // STATE 2: Connected but NOT time synched - wait for time sync packet from client
    if (!isTimeSynched) {
        Serial.println("Waiting for time synchronization packet");
        delay(1000);
        return;
    }

    // STATE 3: Connected and time synched, but NOT YET syncing - prepare sync
    if (isTimeSynched && !isSyncing) {
        Serial.println("Time synchronized! Preparing to send historical data");
        
        // Migrate: temp.csv (with relative timestamps) → data.csv (with real timestamps)
        // Note: migrateTempFiles is called by the BLE callback, so this is already done
        // But we do it here safely in case of edge cases
        
        // Rename data.csv → sync.csv for transmission
        if (storage.prepareDataForSync()) {
            isSyncing = true;
            Serial.println("Ready to send data");
        } else {
            Serial.println("No data to send or error preparing data for sync");
        }
        
        delay(100);
        return;
    }

    // STATE 4: Connected, synched, and currently syncing - send data line by line
    if (isSyncing) {
        String syncData = storage.readSyncFile();
        
        if (syncData.length() > 0) {
            Serial.println("Sending historical data line by line");
            
            // Split and send line by line
            int lineStart = 0;
            while (lineStart < syncData.length()) {
                int lineEnd = syncData.indexOf('\n', lineStart);
                if (lineEnd == -1) lineEnd = syncData.length();
                
                String line = syncData.substring(lineStart, lineEnd);
                if (line.length() > 0) {
                    connection.sendHistoryChunk("HIST:" + line + "\n");
                    delay(40); // Small delay to avoid overwhelming BLE
                }
                lineStart = lineEnd + 1;
            }
            Serial.println("Finished sending data. Waiting for client confirmation");
        }
        
        // Wait for client to confirm receipt via shouldClearStorage flag
        if (connection.shouldClearStorage) {
            storage.clearSyncFile();
            connection.shouldClearStorage = false;
            isSyncing = false;
            Serial.println("Historical data synchronized and deleted!");
        }
    }

    // Send current weight for real-time display (non-blocking)
    connection.updateWeight(loadCell.getWeight());
    
    delay(1000);
}