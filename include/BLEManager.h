#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h> // Notifications
#include <time.h> // to synchronize time via BLE
#include "Storage.h"
#include "BatteryManager.h"
class LoadCell;

void TaskBLEManager(void * pvParameters);  // Expects a pointer to a BLEManager instance as parameter

class BLEManager {
public:
    BLEManager(const char* deviceName = "Hydroholic");

    void begin(Storage* storage, volatile bool* isSynched, LoadCell* loadCell, BatteryManager* batteryManager); // setup

    void updateWeight();
    
    bool isConnected();

    void sendInformation(String chunk);
    void sendScaleFactor();

    volatile bool shouldClearStorage = false; // Flag to indicate when the storage should be cleared after successful sync

private:
    const char* _deviceName;
    BLEServer* _pServer;
    BLECharacteristic* _pWeightChar;
    BLECharacteristic* _pTimeChar;
    Storage * _storage;
    LoadCell * _loadCell = nullptr;
    BatteryManager * _batteryManager = nullptr;
    volatile bool* _isSynched;
    volatile bool _deviceConnected = false;

    // Callbacks to detect connection/disconnection
    class ServerCallbacks : public BLEServerCallbacks {
        BLEManager* _manager;
        public:
            ServerCallbacks(BLEManager* m) : _manager(m) {}
            void onConnect(BLEServer* pServer) override { _manager->_deviceConnected = true; }
            void onDisconnect(BLEServer* pServer) override { 
                _manager->_deviceConnected = false;
                *_manager->_isSynched = false;
                _manager->_pServer->getAdvertising()->start();
            }
    };

    class TimeCallbacks : public BLECharacteristicCallbacks {
        BLEManager* _manager;
        public:
            TimeCallbacks(BLEManager* m) : _manager(m) {}
            void onWrite(BLECharacteristic* pChar) override;
    };
};