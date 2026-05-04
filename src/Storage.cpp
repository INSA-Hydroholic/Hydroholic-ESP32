#include "Storage.h"
#include "BLEManager.h"

Storage::Storage(const char* filename) : _filename(filename) {
    _mutex = xSemaphoreCreateMutex();
}

bool Storage::begin() {
    // Le paramètre 'true' demande à l'ESP32 de formater la Flash 
    // si elle n'est pas déjà prête.
    bool success = LittleFS.begin(true); 
    
    if (success) {
        Serial.println("LittleFS monté avec succès !");
    } else {
        Serial.println("ÉCHEC critique du montage LittleFS...");
    }
    return success;
}


bool Storage::append(const char* filename, KeyValuePair* kvps, size_t numKvps) {
    if (filename == nullptr || kvps == nullptr || numKvps == 0) {
        Serial.println("ERROR : Invalid parameters for append, filename or kvps is null or numKvps is zero");
        return false;
    }

    // Check if we already have a header for this file, if not store the provided keys as the header for future reference
    if (_fileHeaders.find(filename) == _fileHeaders.end()) {
        const char** keys = new const char*[numKvps];
        for (size_t i = 0; i < numKvps; i++) {
            keys[i] = kvps[i].key;
        }
        _fileHeaders[filename] = std::make_pair(keys, numKvps);
    } else {
        // Check if the provided keys match the existing header for this file
        const char** existingKeys = _fileHeaders[filename].first;
        size_t existingNumKvps = _fileHeaders[filename].second;
        if (existingNumKvps != numKvps) {
            Serial.println("ERROR : Header keys count mismatch for file " + String(filename));
            return false;
        }
        for (size_t i = 0; i < numKvps; i++) {
            if (strcmp(existingKeys[i], kvps[i].key) != 0) {
                Serial.println("ERROR : Header keys mismatch for file " + String(filename));
                return false;
            }
        }
    }

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {

        // Ensure the file exists (create if necessary). If open fails, try remounting once.
        if (!LittleFS.exists(filename)) {
            File testFile = LittleFS.open(filename, "w");
            if (testFile) {
                // Append header line with keys if the file was just created
                for (size_t i = 0; i < numKvps; i++) {
                    testFile.print(kvps[i].key);
                    if (i < numKvps - 1) {
                        testFile.print(",");
                    }
                }
                testFile.close();
            } else {
                // Try remounting FS then retry file creation once
                vTaskDelay(20 / portTICK_PERIOD_MS);
                LittleFS.end();
                LittleFS.begin(false);
                vTaskDelay(20 / portTICK_PERIOD_MS);
                testFile = LittleFS.open(filename, "w");
                if (testFile) {
                    // Append header line with keys if the file was just created
                    for (size_t i = 0; i < numKvps; i++) {
                        testFile.print(kvps[i].key);
                        if (i < numKvps - 1) {
                            testFile.print(",");
                        }
                    }
                    testFile.close();
                } else {
                    Serial.println("ERROR : Failed to create file for appending data");
                    xSemaphoreGive(_mutex);
                    return false;
                }
            }
        }

        File file = LittleFS.open(filename, "a");
        if (!file) {
            // Try remounting and retry once
            vTaskDelay(20 / portTICK_PERIOD_MS);
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(20 / portTICK_PERIOD_MS);
            file = LittleFS.open(filename, "a");
        }
        
        if (!file) {
            Serial.printf("ERROR : Failed to create or open file %s\n", filename);
            xSemaphoreGive(_mutex);
            return false;
        }
        
        // Store each value in the CSV file, separated by commas
        for (size_t i = 0; i < numKvps; i++) {
            file.print(kvps[i].value);
            if (i < numKvps - 1) {
                file.print(",");
            }
        }
        file.println();
        file.close();
        xSemaphoreGive(_mutex);
        return true;
    }
    return false;
}

String Storage::readHeader(const char* filename) {
    String headerLine = "";
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        File file = LittleFS.open(filename, "r");
        if (!file) {
            // Try remount once then retry
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            file = LittleFS.open(filename, "r");
        }
        if (file) {
            // Read only the header line (first line) to extract keys
            if (file.available()) {
                headerLine = file.readStringUntil('\n');
            }
            file.close();
        }
        xSemaphoreGive(_mutex);
    }
    return headerLine;
}

String Storage::readContent(const char* filename) {
    String data = "";
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        File file = LittleFS.open(filename, "r");
        if (!file) {
            // Try remount once then retry
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            file = LittleFS.open(filename, "r");
        }
        if (file) {
            bool isFirstLine = true;
            while (file.available()) {
                if (isFirstLine) {
                    // Skip the header line
                    file.readStringUntil('\n');
                    isFirstLine = false;
                    continue;
                }
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
        bool result = false;
        File f = LittleFS.open(_filename, "w");
        if (f) {
            f.close();
            result = true;
        } else {
            // Try remount and retry once
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            f = LittleFS.open(_filename, "w");
            if (f) { f.close(); result = true; }
        }
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
        if (!tempFile || !dataFile) {
            // Try remount and retry once
            if (tempFile) tempFile.close();
            if (dataFile) dataFile.close();
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            tempFile = LittleFS.open("/temp.csv", "r");
            dataFile = LittleFS.open("/data.csv", "a");
        }

        if (!tempFile || !dataFile) {
            if (tempFile) tempFile.close();
            if (dataFile) dataFile.close();
            Serial.println("Error opening files for migration");
            xSemaphoreGive(_mutex);
            return;
        }

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
        // Truncate temp file instead of removing it to avoid permission/creation races
        File t = LittleFS.open("/temp.csv", "w");
        if (t) {
            t.close();
        } else {
            // Try remount and retry once
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            t = LittleFS.open("/temp.csv", "w");
            if (t) t.close();
        }

        xSemaphoreGive(_mutex);
        Serial.println("Migration terminée. temp.csv vidé.");
    }
}

bool Storage::prepareDataForSync() {
    // Rename /data.csv to /sync.csv for sending to client
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        if (LittleFS.exists("/data.csv")) {
            bool ok = LittleFS.rename("/data.csv", "/sync.csv");
            if (!ok) {
                // Try remount and retry rename once
                LittleFS.end();
                LittleFS.begin(false);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                ok = LittleFS.rename("/data.csv", "/sync.csv");
            }
            if (ok) {
                // Create an empty /data.csv immediately so other tasks can continue appending
                File newData = LittleFS.open("/data.csv", "w");
                if (newData) {
                    newData.close();
                } else {
                    // Try remount and retry once
                    LittleFS.end();
                    LittleFS.begin(false);
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    newData = LittleFS.open("/data.csv", "w");
                    if (newData) newData.close();
                }
                xSemaphoreGive(_mutex);
                Serial.println("Fichier renommé en sync.csv pour envoi.");
                return true;
            }
        }
        xSemaphoreGive(_mutex);
    }
    return false;
}


bool Storage::clearSyncFile() {
    // Delete /sync.csv
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        bool result = false;
        File f = LittleFS.open("/sync.csv", "w");
        if (f) {
            f.close();
            result = true;
        } else {
            // Try remount and retry once
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            f = LittleFS.open("/sync.csv", "w");
            if (f) { f.close(); result = true; }
        }
        xSemaphoreGive(_mutex);
        return result;
    }
    return false;
}