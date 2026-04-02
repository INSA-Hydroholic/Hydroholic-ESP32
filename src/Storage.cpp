#include "Storage.h"
#include "ConnectionManager.h"

Storage::Storage(const char* filename) : _filename(filename) {
    _mutex = xSemaphoreCreateMutex();
}

bool Storage::begin() {
    return LittleFS.begin();
}


bool Storage::append(uint32_t timestamp, float value, bool isSynched) {
    _filename = isSynched ? "/data.csv" : "/temp.csv";

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

void Storage::migrateTempFiles(long startTime) {
    // Function to migrate data from temp.csv to data.csv, adjusting timestamps based on startTime
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        if (!LittleFS.exists("/temp.csv")) {
            xSemaphoreGive(_mutex);
            return;
        }

        File tempFile = LittleFS.open("/temp.csv", "r");
        File dataFile = LittleFS.open("/data.csv", "a");

        while (tempFile.available()) {
            String line = tempFile.readStringUntil('\n');
            int commaIndex = line.indexOf(',');
            if (commaIndex != -1) {
                long relativeTime = line.substring(0, commaIndex).toInt();
                String weight = line.substring(commaIndex + 1);
                
                long realTimestamp = startTime + relativeTime;
                dataFile.printf("%u,%s\n", (uint32_t)realTimestamp, weight.c_str());
            }
        }

        tempFile.close();
        dataFile.close();
        LittleFS.remove("/temp.csv"); 
        
        xSemaphoreGive(_mutex);
        Serial.println("Migration terminée. temp.csv supprimé.");
    }
}