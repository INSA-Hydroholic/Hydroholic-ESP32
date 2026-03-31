#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Storage {
public:
    Storage(const char* filename = "/data.txt");
    bool begin();
    bool append(uint32_t timestamp, float value);
    String readAll();
    bool clear();
private:
    const char* _filename;
    SemaphoreHandle_t _mutex;
};
