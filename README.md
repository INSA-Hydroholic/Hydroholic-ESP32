# Hydroholic-ESP32
Repository destined to the embedded part of the project

The code is divided in 3 main sections:

- LoadCell.cpp
    Management of the scale. Responsible for calibrating, averaging and retrieving the scale's measurements

- Storage.cpp
    Handles the file system of the ESP32 using the LittleFS library and semaphores

- ConnectionManager.cpp
    Module responsible for handling the BLE Connection and protocol

The ESP32 controls the HX7