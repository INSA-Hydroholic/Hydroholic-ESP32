#include "Storage.h"

Storage::Storage(const char* filename) : _filename(filename) {
    _mutex = xSemaphoreCreateMutex();
}

bool Storage::begin() {
    return LittleFS.begin();
}


bool Storage::append(uint32_t timestamp, float value) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        File file = LittleFS.open(_filename, "a");
        if (!file) {
            xSemaphoreGive(_mutex);
            return false;
        }
        // Store as: timestamp,value\n
        file.print(timestamp);
        file.print(",");
        file.println(String(value, 2));
        file.close();
        xSemaphoreGive(_mutex);
        return true;
    }
    return false;
}

String Storage::readAll() {
    String data = "";
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        File file = LittleFS.open(_filename, "r");
        if (file) {
            while (file.available()) {
                data += file.readStringUntil('\n');
                data += '\n';
            }
            file.close();
        }
        xSemaphoreGive(_mutex);
    }
    return data;
}

bool Storage::clear() {
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        bool result = LittleFS.remove(_filename);
        xSemaphoreGive(_mutex);
        return result;
    }
    return false;
}
