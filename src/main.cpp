#include <Arduino.h>
#include "LoadCell.h"
#include "ConnectionManager.h"
#include "Storage.h"

// PIN Definitions
#define HX711_DOUT_PIN 4
#define HX711_SCK_PIN 5

ConnectionManager connection("Hydroholic");
Storage stockage("/data.txt");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 2280.0);

void TaskCapteur(void * pvParameters) {
    unsigned long lastSaveTime = 0;

    for(;;) {
        loadCell.measureWeight();
        float currentWeight = loadCell.getWeight();
        
        Serial.print("Poids mesuré (Core 0) : ");
        Serial.println(currentWeight);

        // Stockage local
        time_t now;
        time(&now);
        stockage.append((uint32_t)now, loadCell.getWeight());

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

        float testValeur = millis() / 1000.0; 
        connection.updateWeight(testValeur);
        
        Serial.print("Test Bluetooth - Envoi de : ");
        Serial.println(testValeur);
    }
    delay(1000);
}