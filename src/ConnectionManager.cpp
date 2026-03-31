#include "ConnectionManager.h"
#include <sys/time.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WEIGHT_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_CHAR_UUID      "e3223119-9445-4e96-a402-55369581a030"

ConnectionManager::ConnectionManager(const char* deviceName) : _deviceName(deviceName) {}

void ConnectionManager::TimeCallbacks::onWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.length() > 0) {

        if (value == "OK") {
            Serial.println("Confirmation reçue !");
            // Ici, il faudra prévenir le main.cpp de vider le storage
            // On peut utiliser un flag global ou un callback
            _manager->shouldClearStorage = true;
            return; 
        }
        // Le client envoie l'Epoch Unix sous forme de string
        long epochTime = atol(value.c_str());

        // On synchronise l'horloge interne (RTC) de l'ESP32
        struct timeval tv;
        tv.tv_sec = epochTime; 
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        Serial.print("Heure synchronisée via BLE ! Epoch : ");
        Serial.println(epochTime);
    }
}

void ConnectionManager::begin() {
    BLEDevice::init(_deviceName);
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    BLEService *pService = _pServer->createService(SERVICE_UUID);

    _pWeightChar = pService->createCharacteristic(
        WEIGHT_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY
    );

    // Ajout du descripteur pour activer les notifications (obligatoire pour Web ConnectionManager/iOS)
    _pWeightChar->addDescriptor(new BLE2902());
    _pWeightChar->setValue("-1.0");


    _pTimeChar = pService->createCharacteristic(
        TIME_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pTimeChar->setCallbacks(new TimeCallbacks(this));


    pService->start();
    // Configuration de l'Advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("ConnectionManager : Prêt et visible !");
}

void ConnectionManager::updateWeight(float weight) {
    if (_deviceConnected) {
        char buffer[10];
        dtostrf(weight, 4, 2, buffer); // Conversion float -> texte
        _pWeightChar->setValue(buffer);
        _pWeightChar->notify(); // Envoie la notif au site web
    }
}

bool ConnectionManager::isConnected() {
    return _deviceConnected;
}

void ConnectionManager::sendHistoryChunk(String chunk) {
    if (_deviceConnected) {
        _pWeightChar->setValue(chunk.c_str());
        _pWeightChar->notify();
        // On laisse un petit délai pour que le téléphone puisse digérer
        delay(50); 
    }
}