#include "ConnectionManager.h"
#include <sys/time.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WEIGHT_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_CHAR_UUID      "e3223119-9445-4e96-a402-55369581a030"

ConnectionManager::ConnectionManager(const char* deviceName) : _deviceName(deviceName) {}

void ConnectionManager::TimeCallbacks::onWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.length() > 0) {
        
        // Client sends epoch time unix as string, we convert it to long
        long epochTime = atol(value.c_str());
        long startTime = epochTime - (millis() / 1000);
        
        *_isSynched = true;

        _storage->migrateTempFiles(startTime);

        // We synchronize the ESP32 time with the received epoch time
        struct timeval tv;
        tv.tv_sec = epochTime;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        Serial.print("Heure synchronisée via BLE ! Epoch : ");
        Serial.println(epochTime);
    }
}

void ConnectionManager::begin(Storage* storage, bool* isSynched) {
    this->_storage = storage;
    this->_isSynched = isSynched;
    
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
    _pTimeChar->setCallbacks(new TimeCallbacks(storage, isSynched));


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
