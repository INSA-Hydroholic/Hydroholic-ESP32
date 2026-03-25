#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h> // Notifications

class Bluetooth {
public:
    Bluetooth(const char* deviceName = "Hydroholic");

    void begin(); // setup

    void updateWeight(float weight);
    
    bool isConnected();

    void goToSleep(uint32_t secondes = 15);

private:
    const char* _deviceName;
    BLEServer* _pServer;
    BLECharacteristic* _pCharacteristic;
    bool _deviceConnected = false;

    // Callbacks pour détecter la connexion/déconnexion
    class ServerCallbacks : public BLEServerCallbacks {
        Bluetooth* _manager;
        public:
            ServerCallbacks(Bluetooth* m) : _manager(m) {}
            void onConnect(BLEServer* pServer) override { _manager->_deviceConnected = true; }
            void onDisconnect(BLEServer* pServer) override { 
                _manager->_deviceConnected = false; 
                // On relance le Bluetooth pour qu'il soit à nouveau visible
                pServer->getAdvertising()->start();
            }
    };
};