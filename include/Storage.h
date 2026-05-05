#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <map>

typedef struct key_value_pair {
    const char* key;
    const char* value;
} KeyValuePair;

class Storage {
public:
    Storage(const char* filename = "/tmp.csv");
    bool begin();
    bool append(const char* filename, KeyValuePair* kvps, size_t numKvps);
    String readHeader(const char* filename);
    String readContent(const char* filename);
    bool clear(const char* filename);
    void migrateTempFiles(long startTime);
    bool prepareDataForSync();
    bool clearSyncFile();
private:
    const char* _filename;
    std::map<const char*, std::pair<const char**, size_t>> _fileHeaders;  // Maps a CSV file to a list of header keys it contains to ensure data integrity during appends
    SemaphoreHandle_t _mutex;
};
