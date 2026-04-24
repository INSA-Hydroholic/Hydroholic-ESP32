#include "LoadCell.h"

void LoadCell::resetStabilityState() {
    historyIndex = 0;
    historyCount = 0;
    isStable = false;
    for (int i = 0; i < STABILITY_SAMPLES; i++) {
        history[i] = 0.0f;
    }
}

void LoadCell::begin() {
    pinMode(_enablePin, OUTPUT);
    turnOn(); // Ensure the HX711 is powered on before initialization

    if (_scaleMutex == nullptr) {
        _scaleMutex = xSemaphoreCreateMutex();
    }

    scale.begin(_doutPin, _sckPin);
    scale.set_scale(calibration_factor);
    scale.tare(20); // Tare with more samples for better stability
    emaValue = 0;
    emaInitialized = false;
    resetStabilityState();
    Serial.println("HX711 : Initialisé avec succès");
}

void LoadCell::tare() {
    if (_scaleMutex) {
        xSemaphoreTake(_scaleMutex, portMAX_DELAY);
    }

    turnOn();
    delay(200); // Let the ADC settle before taring
    scale.tare(20);
    emaValue = 0;
    emaInitialized = false;
    resetStabilityState();

    if (_scaleMutex) {
        xSemaphoreGive(_scaleMutex);
    }
}

bool LoadCell::setCalibrationFactor(float factor) {
    if (factor <= 0.0f) {
        return false;
    }

    if (_scaleMutex) {
        xSemaphoreTake(_scaleMutex, portMAX_DELAY);
    }

    calibration_factor = factor;
    scale.set_scale(calibration_factor);
    emaValue = 0;
    emaInitialized = false;
    resetStabilityState();

    if (_scaleMutex) {
        xSemaphoreGive(_scaleMutex);
    }

    return true;
}

float LoadCell::getCalibrationFactor() {
    float factor = calibration_factor;

    if (_scaleMutex) {
        xSemaphoreTake(_scaleMutex, portMAX_DELAY);
        factor = calibration_factor;
        xSemaphoreGive(_scaleMutex);
    }

    return factor;
}

void LoadCell::measureWeight() {  // This method should be called every second or so
    if (_scaleMutex) {
        xSemaphoreTake(_scaleMutex, portMAX_DELAY);
    }

    // Read a reduced number of samples to avoid long blocking periods
    for (int i = 0; i < NUM_SAMPLES; i++) {
        samples[i] = scale.get_units(3); // Smaller per-read averaging for responsiveness
    }

    // Sort the samples to find the median
    std::sort(samples, samples + NUM_SAMPLES);
    float median = samples[NUM_SAMPLES / 2]; // Middle value after sorting (median)

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

    if (_scaleMutex) {
        xSemaphoreGive(_scaleMutex);
    }
}

float LoadCell::getWeight() const {
    float weight = emaValue;

    if (_scaleMutex) {
        xSemaphoreTake(_scaleMutex, portMAX_DELAY);
        weight = emaValue;
        xSemaphoreGive(_scaleMutex);
    }

    return weight;
}

bool LoadCell::isStableWeight() const {
    bool stable = isStable;

    if (_scaleMutex) {
        xSemaphoreTake(_scaleMutex, portMAX_DELAY);
        stable = isStable;
        xSemaphoreGive(_scaleMutex);
    }

    return stable;
}

void LoadCell::turnOn() {
    digitalWrite(_enablePin, HIGH); // Enable the HX711
}

void LoadCell::turnOff() {
    digitalWrite(_enablePin, LOW); // Disable the HX711 to save power
}