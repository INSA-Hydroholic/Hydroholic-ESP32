# Hydroholic-ESP32
Repository destined to the embedded part of the project

The code is divided in the following sections:

- LoadCell.cpp
    Management of the scale. Responsible for calibrating, averaging and retrieving the scale's measurements. Uses exponentially weighted moving average (EWMA)

- Storage.cpp
    Handles the file system of the ESP32 using the LittleFS library and semaphores

- ConnectionManager.cpp
    Module responsible for handling the BLE Connection and protocol. Sends requested information to app.

- BatteryManager.cpp
    Handles battery management (read)


# Code Logic

The ESP32 measures the weight using the load cell on boot. It starts registering the measurements using its internal clock.

If it connects to a BLE Client (cellphone, computer), it waits for a time sync packet from the client. Once received, the ESP32 updates its internal clock to match the client's. It then calls migrateTempFiles to update the old time stamps it stored in temp.csv and moving them to data.csv. Once that is done, it renames that file to sync.csv and starts sending information to the client line by line.

The sending of information is handled by one core, while the writing is handled by another.