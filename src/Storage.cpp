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


bool Storage::append(uint32_t timestamp, float value, bool isSynched) {
    const char* targetFile = isSynched ? "/data.csv" : "/temp.csv";

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {

        // Ensure the file exists (create if necessary). If open fails, try remounting once.
        if (!LittleFS.exists(targetFile)) {
            File testFile = LittleFS.open(targetFile, "w");
            if (testFile) {
                testFile.close();
            } else {
                // Try remounting FS then retry file creation once
                vTaskDelay(20 / portTICK_PERIOD_MS);
                LittleFS.end();
                LittleFS.begin(false);
                vTaskDelay(20 / portTICK_PERIOD_MS);
                testFile = LittleFS.open(targetFile, "w");
                if (testFile) testFile.close();
            }
        }

        File file = LittleFS.open(targetFile, "a");
        if (!file) {
            // Try remounting and retry once
            vTaskDelay(20 / portTICK_PERIOD_MS);
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(20 / portTICK_PERIOD_MS);
            file = LittleFS.open(targetFile, "a");
        }
        
        if (!file) {
            Serial.printf("Erreur critique : Impossible de créer %s\n", targetFile);
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
        if (!file) {
            // Try remount once then retry
            LittleFS.end();
            LittleFS.begin(false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
            file = LittleFS.open(_filename, "r");
        }
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