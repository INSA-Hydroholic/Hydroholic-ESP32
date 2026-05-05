#include "LoadCell.h"

void TaskLoadCell(void * pvParameters) {
    loadcell_task_parameters_t* params = static_cast<loadcell_task_parameters_t*>(pvParameters);
    LoadCell* loadCell = params->loadCell;
    Storage* dataStorage = params->storage;
    RTC_DS1307 *rtc = params->rtc;
    unsigned long lastSaveTime = 0;
    for(;;) {
        if (millis() - lastSaveTime >= LOADCELL_MEASURE_INTERVAL) {
            loadCell->measureWeight();
        }

        if (millis() - lastSaveTime >= LOADCELL_SAVE_DATA_INTERVAL) {
            lastSaveTime = millis();
            float weight = loadCell->getWeight();
            DateTime now = rtc->now();
            String epochStr = String(now.unixtime());
            String weightStr = String(weight, 2); // Convert weight to string with 2 decimal places
            KeyValuePair kvps[] = {
                {"epoch", epochStr.c_str()},
                {"weight", weightStr.c_str()},
                {"stable", loadCell->isStableWeight() ? "1" : "0"}
            };
            dataStorage->append(LOADCELL_DATA_FILE, kvps, 3);
            Serial.print("Saved weight data: ");
            Serial.print(weight);
            Serial.print(" g at time ");
            Serial.println(now.timestamp());
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Run every 500 ms
    }
}

void LoadCell::resetStabilityState() {
    // This method is called from mutex protected sections, no need to take mutex here
    historyIndex = 0;
    historyCount = 0;
    isStable = false;
    for (int i = 0; i < STABILITY_SAMPLES; i++) {
        history[i] = 0.0f;
    }
}

void LoadCell::begin() {
    if (_enablePin >= 0) {
        pinMode(_enablePin, OUTPUT);
        turnOn(); // Ensure the HX711 is powered on before initialization
    }

    _scaleMutex = xSemaphoreCreateMutex();

    scale.begin(_doutPin, _sckPin);
    scale.set_scale(calibration_factor);
    delay(200);     // Let the ADC settle before taring
    scale.tare(20); // Tare with more samples for better stability
    emaValue = 0;
    emaInitialized = false;
    resetStabilityState();
    Serial.println("HX711 : board initialized and tared.");
}

void LoadCell::tare() {
    if (xSemaphoreTake(_scaleMutex, portMAX_DELAY) == pdTRUE) {
        turnOn();
        delay(200); // Let the ADC settle before taring
        scale.tare(20);
        emaValue = 0;
        emaInitialized = false;
        resetStabilityState();

        xSemaphoreGive(_scaleMutex);
    } else {
        Serial.println("Error : Failed to take scale mutex for tare");
    }
}

bool LoadCell::setCalibrationFactor(float factor) {
    if (factor <= 0.0f) {
        return false;
    }

    if (xSemaphoreTake(_scaleMutex, portMAX_DELAY) == pdTRUE) {
        calibration_factor = factor;
        scale.set_scale(calibration_factor);
        emaValue = 0;
        emaInitialized = false;
        resetStabilityState();

        xSemaphoreGive(_scaleMutex);

        return true;
    } else {
        Serial.println("Error : Failed to take scale mutex for setCalibrationFactor");
        return false;
    }
}

float LoadCell::getCalibrationFactor() {
    float factor = calibration_factor;

    if (xSemaphoreTake(_scaleMutex, portMAX_DELAY) == pdTRUE) {
        factor = calibration_factor;
        xSemaphoreGive(_scaleMutex);
    } else {
        Serial.println("Error : Failed to take scale mutex for getCalibrationFactor");
    }

    return factor;
}

void LoadCell::measureWeight() {  // This method should be called every second or so
    if (xSemaphoreTake(_scaleMutex, portMAX_DELAY) == pdTRUE) {
        // Read a reduced number of samples to avoid long blocking periods
        for (int i = 0; i < LOADCELL_NUM_SAMPLES; i++) {
            samples[i] = scale.get_units(3); // Smaller per-read averaging for responsiveness
        }

        // Sort the samples to find the median
        std::sort(samples, samples + LOADCELL_NUM_SAMPLES);
        float median = samples[LOADCELL_NUM_SAMPLES / 2]; // Middle value after sorting (median)

        // Update EMA and history under the same lock used by tare/calibration updates.
        if (!emaInitialized) {
            emaValue = median; // Initialize EMA with the first median value
            emaInitialized = true;
        } else {
            emaValue = (EMA_ALPHA * median) + ((1 - EMA_ALPHA) * emaValue);
        }

        history[historyIndex] = emaValue;
        historyIndex = (historyIndex + 1) % STABILITY_SAMPLES; // Circular buffer
        if (historyCount < STABILITY_SAMPLES) {
            historyCount++;
        }

        // Do not report stable until we filled the full analysis window.
        if (historyCount < STABILITY_SAMPLES) {
            isStable = false;
        } else {
            isStable = true;
            for (int i = 0; i < STABILITY_SAMPLES; i++) {
                if (fabs(history[i] - emaValue) > STABILITY_THRESHOLD) {
                    isStable = false;
                    break;
                }
            }
        }

        xSemaphoreGive(_scaleMutex);
    } else {
        Serial.println("Error : Failed to take scale mutex for measureWeight");
    }
}

float LoadCell::getWeight() const {
    float weight = emaValue;

    if (xSemaphoreTake(_scaleMutex, portMAX_DELAY) == pdTRUE) {
        weight = emaValue;
        xSemaphoreGive(_scaleMutex);
    } else {
        Serial.println("Error : Failed to take scale mutex for getWeight");
    }

    return weight;
}

bool LoadCell::isStableWeight() const {
    bool stable = isStable;

    if (xSemaphoreTake(_scaleMutex, portMAX_DELAY) == pdTRUE) {
        stable = isStable;
        xSemaphoreGive(_scaleMutex);
    } else {
        Serial.println("Error : Failed to take scale mutex for isStableWeight");
    }

    return stable;
}

void LoadCell::turnOn() {
    if (_enablePin >= 0) {
        digitalWrite(_enablePin, HIGH); // Enable the HX711
    }
}

void LoadCell::turnOff() {
    if (_enablePin >= 0) {
        digitalWrite(_enablePin, LOW); // Disable the HX711 to save power
    }
}