#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "LoadCell.h"
#include "Wrapper.h"
#include <BLE2902.h>

// PIN Definitions 
#define HX711_DOUT_PIN  4
#define HX711_SCK_PIN   5


// Global variables shared between tasks
const float calibration_factor = 2280.0; // Adjust based on calibration
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, calibration_factor);
BLECharacteristic *pCharacteristic;

// IDs BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- TÂCHE POUR LE CAPTEUR DE POIDS (S'exécutera sur le Core 0) ---
void TaskCapteurPoids(void * pvParameters) {
  Serial.print("Tâche Capteur lancée sur le cœur : ");
  Serial.println(xPortGetCoreID());

  for(;;) { // Boucle infinie pour cette tâche
    loadCell.measureWeight();
    
    Serial.print("Sensor reading : ");
    Serial.println(loadCell.getWeight());

    // TODO : store the readings in a thread-safe way on flash to be accessed by the BLE task on the other core

    // Delay to let other tasks run (especially important if ever run on the same core as BLE)
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);

  // 1. Setup Bluetooth (BLE) on Core 1
  BLEDevice::init("Hydroholic");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("-1.0"); // Start with an invalid weight to indicate no reading yet
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // 2. Setup Load Cell (HX711) on Core 0
  loadCell.begin();
  xTaskCreatePinnedToCore(
    TaskCapteurPoids,   // Fonction de la tâche
    "LecturePoids",     // Nom de la tâche
    10000,              // Taille de la pile (Stack size)
    NULL,               // Paramètres d'entrée
    1,                  // Priorité (1 est basse, plus c'est haut, plus c'est prioritaire)
    NULL,               // Handle de la tâche
    0                   // CŒUR : 0 (Le Bluetooth est souvent sur le 1 ou géré par le système)
  );

  Serial.println("System ready on Core 1 for BLE and Core 0 for sensor reading.");
}

void loop() {
  // La loop tourne sur le Core 1 par défaut
  // On met à jour la valeur Bluetooth avec le poids lu sur l'autre cœur
  char chainePoids[10];
  dtostrf(poidsActuel, 4, 2, chainePoids); // Convertit le float en texte
  
  pCharacteristic->setValue(chainePoids);
  pCharacteristic->notify(); // Prévient le téléphone que la valeur a changé

  delay(1000); // On rafraîchit le Bluetooth toutes les secondes
}