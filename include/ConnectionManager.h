#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h> // Notifications

class ConnectionManager {
public:
    ConnectionManager(const char* deviceName = "Hydroholic");

    void begin(); // setup

    void updateWeight(float weight);
    
    bool isConnected();

private:
    const char* _deviceName;
    BLEServer* _pServer;
    BLECharacteristic* _pCharacteristic;
    bool _deviceConnected = false;

    // Callbacks to detect connection/disconnection
    class ServerCallbacks : public BLEServerCallbacks {
        ConnectionManager* _manager;
        public:
            ServerCallbacks(ConnectionManager* m) : _manager(m) {}
            void onConnect(BLEServer* pServer) override { _manager->_deviceConnected = true; }
            void onDisconnect(BLEServer* pServer) override { 
                _manager->_deviceConnected = false; 
                // On relance le ConnectionManager pour qu'il soit à nouveau visible
                pServer->getAdvertising()->start();
            }
    };
};