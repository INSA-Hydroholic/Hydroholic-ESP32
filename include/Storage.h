#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Storage {
public:
    Storage(const char* filename = "/tmp.csv");
    bool begin();
    bool append(uint32_t timestamp, float value, bool isSynched);
    String readAll();
    bool clear();
    void migrateTempFiles(long startTime);
    bool prepareDataForSync();
    bool clearSyncFile();
private:
    const char* _filename;
    SemaphoreHandle_t _mutex;
};
