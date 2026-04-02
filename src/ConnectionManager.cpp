#include "ConnectionManager.h"
#include <sys/time.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WEIGHT_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_CHAR_UUID      "e3223119-9445-4e96-a402-55369581a030"

ConnectionManager::ConnectionManager(const char* deviceName) : _deviceName(deviceName) {}

void ConnectionManager::TimeCallbacks::onWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.length() > 0) {
        // Client sends Epoch time as a string, we convert it to long
        long epochTime = atol(value.c_str());
        long startTime = epochTime - (millis() / 1000);
        
        *_manager->_isSynched = true;

        _manager->_storage->migrateTempFiles(startTime);

        // We synchronize the ESP32 time with the received epoch time
        struct timeval tv;
        tv.tv_sec = epochTime;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        Serial.print("Time synchronized. Epoch : ");
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

    // Add a descriptor to enable notifications on the client side
    _pWeightChar->addDescriptor(new BLE2902());
    _pWeightChar->setValue("-1.0");


    _pTimeChar = pService->createCharacteristic(
    TIME_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY 
    );


    _pTimeChar->addDescriptor(new BLE2902()); 

    _pTimeChar->setCallbacks(new TimeCallbacks(this));

    pService->start();
    // Configuration de l'Advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("ConnectionManager ready and advertising");
}

void ConnectionManager::updateWeight(float weight) {
    if (_deviceConnected) {
        char buffer[10];
        dtostrf(weight, 4, 2, buffer); // Conversion float -> text
        _pWeightChar->setValue(buffer);
        _pWeightChar->notify(); // Send notification to client
    }
}

bool ConnectionManager::isConnected() {
    return _deviceConnected;
}

void ConnectionManager::sendHistoryChunk(String chunk) {
    if (_deviceConnected) {
        _pWeightChar->setValue(chunk.c_str());
        _pWeightChar->notify();

        // Small delay to ensure the client has time to process the notification
        delay(50); 
    }
}