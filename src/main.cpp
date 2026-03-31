#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "Storage.h"

// PIN Definitions
#define HX711_DOUT_PIN 4
#define HX711_SCK_PIN 5

// Global variables shared between tasks
ConnectionManager connection("Hydroholic");

const char* STORAGE_FILENAME = "/data.txt";
Storage stockage(STORAGE_FILENAME);

const float CALIBRATION_FACTOR = 2280.0; // Adjust this value based on your calibration
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, CALIBRATION_FACTOR);

// --- Tâche Capteur (Core 0) ---
void TaskCapteur(void * pvParameters) {
    for(;;) {
        loadCell.measureWeight();
        
        Serial.print("Measured weight : ");
        Serial.println(loadCell.getWeight());

        // Stockage local
        uint32_t timestamp = millis() / 1000; // Timestamp en secondes
        stockage.append(timestamp, loadCell.getWeight());

        vTaskDelay(1000 / portTICK_PERIOD_MS); // 1 reading per second
    }
}

void setup() {
    Serial.begin(115200);

    // 1. Setup sensor and Bluetooth
    loadCell.begin();
    connection.begin();

    // 2. Create task for sensor reading on Core 0
    xTaskCreatePinnedToCore(
        TaskCapteur, 
        "TaskCapteur", 
        10000, 
        NULL, 
        1, 
        NULL, 
        0
    );

    Serial.println("System initialized!");
}

void loop() {
    // La loop tourne sur le Core 1 par défaut
    // On met à jour le Bluetooth toutes les secondes
    if (connection.isConnected()) {
        connection.updateWeight(loadCell.getWeight());
    }
    
    delay(1000);
}