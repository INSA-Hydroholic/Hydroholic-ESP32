#include "ConnectionManager.h"
#include "LoadCell.h"
#include <sys/time.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WEIGHT_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_CHAR_UUID      "e3223119-9445-4e96-a402-55369581a030"

ConnectionManager::ConnectionManager(const char* deviceName) : _deviceName(deviceName) {}

/**
 * @brief Callback function for handling write requests to the time characteristic
 * 
 * @param pChar Pointer to the BLE characteristic that received the write request. The value is expected to be a string representing the epoch time or "OK" for confirmation.
 */
void ConnectionManager::TimeCallbacks::onWrite(BLECharacteristic* pChar) {
    std::string value = pChar->getValue();
    if (value.length() > 0) {

        if (value == "OK") {
            Serial.println("Confirmation reçue : Le client a bien tout enregistré !");
            _manager->shouldClearStorage = true; 
            return; 
        }
        if (value == "TARE") {
            if (_manager->_loadCell) {
                _manager->_loadCell->tare();
                Serial.println("Commande reçue : tare de la balance effectuée.");
            }
            return;
        }

        if (value == "GET_BATTERY" || value == "BATTERY?") {
            if (_manager->_loadCell) {
                _manager->sendHistoryChunk("BATTERY:" + String(_manager->_batteryManager->getBatteryLevel()));
                Serial.println("Commande reçue : envoi du niveau de batterie.");
            }
            return;
        }

        if (value == "GET_SCALE" || value == "SCALE?") {
            _manager->sendScaleFactor();
            Serial.println("Commande reçue : envoi du facteur de calibration.");
            return;
        }

        const std::string setScalePrefix = "SET_SCALE:";
        if (value.rfind(setScalePrefix, 0) == 0) {
            if (!_manager->_loadCell) {
                Serial.println("Erreur : loadCell indisponible pour SET_SCALE.");
                return;
            }

            std::string factorPart = value.substr(setScalePrefix.length());
            char* endPtr = nullptr;
            float newFactor = strtof(factorPart.c_str(), &endPtr);

            if (endPtr == factorPart.c_str() || *endPtr != '\0' || newFactor <= 0.0f) {
                Serial.println("SET_SCALE invalide : valeur non numerique ou <= 0.");
                if (_manager->_deviceConnected) {
                    _manager->_pWeightChar->setValue("SCALE:ERROR");
                    _manager->_pWeightChar->notify();
                }
                return;
            }

            bool updated = _manager->_loadCell->setCalibrationFactor(newFactor);
            if (!updated) {
                Serial.println("Echec SET_SCALE: facteur invalide.");
                if (_manager->_deviceConnected) {
                    _manager->_pWeightChar->setValue("SCALE:ERROR");
                    _manager->_pWeightChar->notify();
                }
                return;
            }
            // Update the stored calibration factor in config.csv
            File configFile = LittleFS.open("/config.csv", "w");
            if (configFile) {
                configFile.printf("CALIB_FACTOR:%.6f\n", newFactor);
                configFile.close();
            } else {
                Serial.println("ERREUR : Impossible d'ouvrir config.csv pour mise à jour du facteur de calibration.");
            }

            Serial.print("Nouveau facteur de calibration applique: ");
            Serial.println(newFactor, 6);
            _manager->sendScaleFactor();
            return;
        }

        // Client sends Epoch UNIX time as a string, we convert it to long
        char* endPtr = nullptr;
        long epochTime = strtol(value.c_str(), &endPtr, 10);
        if (endPtr == value.c_str() || *endPtr != '\0') {
            Serial.println("Commande BLE inconnue ignorée.");
            return;
        }
        long startTime = epochTime - (millis() / 1000);
        
        // We synchronize the ESP32 time with the received epoch time
        struct timeval tv;
        tv.tv_sec = epochTime;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        *_manager->_isSynched = true;

        _manager->_storage->migrateTempFiles(startTime);

        Serial.print("Time synchronized. Epoch : ");
        Serial.println(epochTime);
    }
}

void ConnectionManager::begin(Storage* storage, volatile bool* isSynched, LoadCell* loadCell, BatteryManager* batteryManager) {
    this->_storage = storage;
    this->_loadCell = loadCell;
    this->_isSynched = isSynched;
    this->_batteryManager = batteryManager;

    BLEDevice::init(_deviceName);
    BLEDevice::setMTU(185);
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

void ConnectionManager::sendScaleFactor() {
    if (!_deviceConnected || !_loadCell) {
        return;
    }

    char payload[40];
    float factor = _loadCell->getCalibrationFactor();
    snprintf(payload, sizeof(payload), "SCALE:%.6f", factor);
    _pWeightChar->setValue(payload);
    _pWeightChar->notify();
}