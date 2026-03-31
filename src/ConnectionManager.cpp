#include "ConnectionManager.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

ConnectionManager::ConnectionManager(const char* deviceName) : _deviceName(deviceName) {}

void ConnectionManager::begin() {
    BLEDevice::init(_deviceName);
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    BLEService *pService = _pServer->createService(SERVICE_UUID);

    _pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY
    );

    // Ajout du descripteur pour activer les notifications (obligatoire pour Web ConnectionManager/iOS)
    _pCharacteristic->addDescriptor(new BLE2902());
    _pCharacteristic->setValue("-1.0");

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
        _pCharacteristic->setValue(buffer);
        _pCharacteristic->notify(); // Envoie la notif au site web
    }
}

bool ConnectionManager::isConnected() {
    return _deviceConnected;
}
