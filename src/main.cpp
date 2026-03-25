#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "LoadCell.h"
#include <BLE2902.h>

// Variables globales pour le partage de données entre les cœurs
float poidsActuel = 0.0;
BLECharacteristic *pCharacteristic;

// Identifiants BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- TÂCHE POUR LE CAPTEUR DE POIDS (S'exécutera sur le Core 0) ---
void TaskCapteurPoids(void * pvParameters) {
  Serial.print("Tâche Capteur lancée sur le cœur : ");
  Serial.println(xPortGetCoreID());

  for(;;) { // Boucle infinie pour cette tâche
    // ICI : Ton code pour lire le HX711 (capteur de poids)
    // Simulation d'une lecture :
    poidsActuel = random(0, 1000) / 10.0; 
    
    Serial.print("Lecture capteur : ");
    Serial.println(poidsActuel);

    // Très important : laisser un petit délai pour que le système respire
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);

  // 1. Initialisation du Bluetooth
  BLEDevice::init("Hydroholic");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("0.0");
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID); // <--- LIGNE OBLIGATOIRE
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // 2. CRÉATION DE LA TÂCHE DÉDIÉE SUR LE CORE 0
  xTaskCreatePinnedToCore(
    TaskCapteurPoids,   // Fonction de la tâche
    "LecturePoids",     // Nom de la tâche
    10000,              // Taille de la pile (Stack size)
    NULL,               // Paramètres d'entrée
    1,                  // Priorité (1 est basse, plus c'est haut, plus c'est prioritaire)
    NULL,               // Handle de la tâche
    0                   // CŒUR : 0 (Le Bluetooth est souvent sur le 1 ou géré par le système)
  );

  Serial.println("Système prêt et Dual-Core activé !");
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