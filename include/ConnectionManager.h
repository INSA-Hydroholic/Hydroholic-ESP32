#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h> // Notifications
#include <time.h> // to synchronize time via BLE
#include "Storage.h"

class ConnectionManager {
public:
    ConnectionManager(const char* deviceName = "Hydroholic");

    void begin(Storage* storage, bool* isSynched); // setup

    void updateWeight(float weight);
    
    bool isConnected();

private:
    const char* _deviceName;
    BLEServer* _pServer;
    BLECharacteristic* _pWeightChar;
    BLECharacteristic* _pTimeChar;
    Storage * _storage;
    bool* _isSynched;
    bool _deviceConnected = false;

    // Callbacks to detect connection/disconnection
    class ServerCallbacks : public BLEServerCallbacks {
        ConnectionManager* _manager;
        public:
            ServerCallbacks(ConnectionManager* m) : _manager(m) {}
            void onConnect(BLEServer* pServer) override { _manager->_deviceConnected = true; }
            void onDisconnect(BLEServer* pServer) override { 
                _manager->_deviceConnected = false;
                *_manager->_isSynched = false;
                pServer->getAdvertising()->start();
            }
    };

    class TimeCallbacks : public BLECharacteristicCallbacks {
        public:
            Storage* _storage;
            bool* _isSynched;
            TimeCallbacks(Storage* storage, bool* isSynched) : _storage(storage), _isSynched(isSynched) {}
            void onWrite(BLECharacteristic* pChar) override; // Appelée quand le client écrit
    };
};