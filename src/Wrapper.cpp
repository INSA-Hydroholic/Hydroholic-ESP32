#include "Wrapper.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Wrapper::Wrapper(const char* deviceName) : _deviceName(deviceName) {}

void Wrapper::begin() {
    BLEDevice::init(_deviceName);
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    BLEService *pService = _pServer->createService(SERVICE_UUID);

    _pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY
    );

    // Ajout du descripteur pour activer les notifications (obligatoire pour Web Bluetooth/iOS)
    _pCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    _pServer->getAdvertising()->start();
}

bool Wrapper::isConnected() {
    return _deviceConnected;
}

void Wrapper::goToSleep(uint32_t secondes) {
    Serial.println("Entering sleep mode");
    
    if (secondes > 0) {
        esp_sleep_enable_timer_wakeup(secondes * 1000000ULL);
    }

    Serial.flush(); 
    esp_deep_sleep_start();
}