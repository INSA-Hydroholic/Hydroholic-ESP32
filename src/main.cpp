#include <Arduino.h>
#include "LoadCell.h"
#include "Bluetooth.h"

// Variables globales
Bluetooth gourde("Hydroholic");
LoadCell loadCell(4, 5, 2280.0); // DOUT=4, SCK=5
float poidsActuel = 0.0;

// --- Tâche Capteur (Core 0) ---
void TaskCapteur(void * pvParameters) {
    for(;;) {
        loadCell.measureWeight();
        poidsActuel = loadCell.getWeight();
        
        Serial.print("Poids mesuré : ");
        Serial.println(poidsActuel);

        vTaskDelay(100 / portTICK_PERIOD_MS); // 10 lectures par seconde
    }
}

void setup() {
    Serial.begin(115200);

    // 1. Init Matériel
    loadCell.begin();
    gourde.begin();

    // 2. Lancer la tâche sur le Core 0
    xTaskCreatePinnedToCore(
        TaskCapteur, 
        "TaskCapteur", 
        10000, 
        NULL, 
        1, 
        NULL, 
        0
    );

    Serial.println("Système Hydroholic lancé !");
}

void loop() {
    // La loop tourne sur le Core 1 par défaut
    // On met à jour le Bluetooth toutes les secondes
    if (gourde.isConnected()) {
        gourde.updateWeight(poidsActuel);
    }
    
    delay(1000);
}