#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h> // Notifications
#include <time.h> // to synchronize time via BLE
#include "Storage.h"

class LoadCell;

class ConnectionManager {
public:
    ConnectionManager(const char* deviceName = "Hydroholic");

    void begin(Storage* storage, volatile bool* isSynched, LoadCell* loadCell); // setup

    void updateWeight(float weight);
    
    bool isConnected();

    void sendHistoryChunk(String chunk);
    void sendScaleFactor();

    volatile bool shouldClearStorage = false; // Flag to indicate when the storage should be cleared after successful sync

private:
    const char* _deviceName;
    BLEServer* _pServer;
    BLECharacteristic* _pWeightChar;
    BLECharacteristic* _pTimeChar;
    Storage * _storage;
    LoadCell * _loadCell = nullptr;
    volatile bool* _isSynched;
    volatile bool _deviceConnected = false;

    // Callbacks to detect connection/disconnection
    class ServerCallbacks : public BLEServerCallbacks {
        ConnectionManager* _manager;
        public:
            ServerCallbacks(ConnectionManager* m) : _manager(m) {}
            void onConnect(BLEServer* pServer) override { _manager->_deviceConnected = true; }
            void onDisconnect(BLEServer* pServer) override { 
                _manager->_deviceConnected = false;
                *_manager->_isSynched = false;
                _manager->_pServer->getAdvertising()->start();
            }
    };

    class TimeCallbacks : public BLECharacteristicCallbacks {
        ConnectionManager* _manager;
        public:
            TimeCallbacks(ConnectionManager* m) : _manager(m) {}
            void onWrite(BLECharacteristic* pChar) override;
    };
};