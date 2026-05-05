#include "BLEManager.h"
#include "LoadCell.h"
#include <sys/time.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMM_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_CHAR_UUID      "e3223119-9445-4e96-a402-55369581a030"

void TaskBLEManager(void * pvParameters) {
    ble_task_parameters_t* params = static_cast<ble_task_parameters_t*>(pvParameters);
    BLEManager* manager = params->manager;
    Storage* dataStorage = params->storage;
    LoadCell* loadCell = params->loadCell;
    BatteryManager* batteryManager = params->batteryManager;

    manager->begin(dataStorage, loadCell, batteryManager);

    // Volatile variables are used to avoid compiler optimizations that could interfere with the state machine logic, since these variables can be modified by both the BLE callbacks and this task loop.
    volatile bool isTimeSynched = false;
    manager->setTimeSynched(&isTimeSynched);
    volatile bool isSyncing = false;
    volatile bool isWaitingForConfirm = false;
    for (;;) {
        if (!manager->isConnected()) {
            isSyncing = false;
            isWaitingForConfirm = false;
            // Blink builtin LED to indicate waiting for manager
            digitalWrite(2, millis() / 500 % 2);
            delay(100);
            continue;
        }
        digitalWrite(2, LOW); // Turn off LED when connected

        // STATE 2 : Attente de l'heure
        if (!isTimeSynched) {
            delay(1000);
            continue;
        }

        // STATE 3 : Préparation (On n'entre ici que si on n'est pas déjà en train de synchro)
        if (isTimeSynched && !isSyncing && !isWaitingForConfirm) {
            if (dataStorage->prepareDataForSync()) {
                isSyncing = true;
                Serial.println("Début de l'envoi...");
            } else {
                // Pas de données à envoyer, on passe directement en mode "prêt"
                isWaitingForConfirm = true; 
            }
        }

        // STATE 4 : Envoi streaming
        if (isSyncing) {
            File f = LittleFS.open("/sync.csv", "r"); 
            if (f) {
                Serial.println("Envoi sécurisé de l'historique...");
                while (f.available()) {
                    String line = f.readStringUntil('\n');
                    if (line.length() > 0) {
                        manager->sendInformation("HIST:" + line + "\n");
                        delay(50); 
                    }
                }
                f.close();
                isSyncing = false;
                isWaitingForConfirm = true; 
                Serial.println("Envoi fini. Attente du OK client...");
            } else {
                // SI LE FICHIER N'EXISTE PAS OU NE PEUT PAS S'OUVRIR
                Serial.println("Erreur : Impossible d'ouvrir sync.csv, abandon de la synchro.");
                isSyncing = false; 
            }
        }

        // STATE 5 : Réception du OK (Vider le stockage)
        if (manager->shouldClearStorage) {
            manager->shouldClearStorage = false; 
            if (LittleFS.exists("/sync.csv")) {
                dataStorage->clearSyncFile();
                Serial.println("Fichier de synchro supprimé !");
            }
            isWaitingForConfirm = false; 
        }

        // Poids en temps réel
        manager->updateWeight();
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Run every 1 second
    }
}

BLEManager::BLEManager(const char* deviceName) : _deviceName(deviceName) {}

/**
 * @brief Callback function for handling write requests to the time characteristic
 * 
 * @param pChar Pointer to the BLE characteristic that received the write request. The value is expected to be a string representing the epoch time or "OK" for confirmation.
 */
void BLEManager::TimeCallbacks::onWrite(BLECharacteristic* pChar) {
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
                _manager->sendInformation("BATTERY:" + String(_manager->_batteryManager->getBatteryLevel()));
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

        *_manager->_isTimeSynched = true;

        _manager->_storage->migrateTempFiles(startTime);

        Serial.print("Time synchronized. Epoch : ");
        Serial.println(epochTime);
    }
}

void BLEManager::begin(Storage* storage, LoadCell* loadCell, BatteryManager* batteryManager) {
    this->_storage = storage;
    this->_loadCell = loadCell;
    this->_batteryManager = batteryManager;

    BLEDevice::init(_deviceName);
    BLEDevice::setMTU(185);
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    BLEService *pService = _pServer->createService(SERVICE_UUID);

    _pWeightChar = pService->createCharacteristic(
        COMM_CHAR_UUID,
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
    
    Serial.println("BLEManager ready and advertising");
}

void BLEManager::updateWeight() {
    float weight = -1.f;
    if (_loadCell) {
        weight = _loadCell->getWeight();
    } else {
        Serial.println("Warning: LoadCell instance not available in BLEManager, cannot update weight.");
        return;
    }

    if (_deviceConnected) {
        char buffer[10];
        dtostrf(weight, 4, 2, buffer); // Conversion float -> text
        _pWeightChar->setValue(buffer);
        _pWeightChar->notify(); // Send notification to client
    }
}

bool BLEManager::isConnected() {
    return _deviceConnected;
}

volatile bool* BLEManager::isTimeSynched() {
    return _isTimeSynched;
}

void BLEManager::setTimeSynched(volatile bool* isTimeSynched) {
    _isTimeSynched = isTimeSynched;
}

void BLEManager::sendInformation(String chunk) {
    if (_deviceConnected) {
        _pWeightChar->setValue(chunk.c_str());
        _pWeightChar->notify();

        // Small delay to ensure the client has time to process the notification
        delay(50); 
    }
}

void BLEManager::sendScaleFactor() {
    if (!_deviceConnected || !_loadCell) {
        return;
    }

    char payload[40];
    float factor = _loadCell->getCalibrationFactor();
    snprintf(payload, sizeof(payload), "SCALE:%.6f", factor);
    _pWeightChar->setValue(payload);
    _pWeightChar->notify();
}