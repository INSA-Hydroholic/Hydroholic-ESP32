#include "LoadCell.h"

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

void LoadCell::measureWeight() {
        if (_scaleMutex) {
                xSemaphoreTake(_scaleMutex, portMAX_DELAY);
        }

        // Read a reduced number of samples to avoid long blocking periods
        for (int i = 0; i < NUM_SAMPLES; i++) {
                samples[i] = scale.get_units(3); // Smaller per-read averaging for responsiveness
        }

        if (_scaleMutex) {
                xSemaphoreGive(_scaleMutex);
        }

        // Sort the samples to find the median
        std::sort(samples, samples + NUM_SAMPLES);
        float median = samples[NUM_SAMPLES / 2]; // Middle value after sorting (median)

    // Update EMA value
    if (!emaInitialized) {
        emaValue = median; // Initialize EMA with the first median value
        emaInitialized = true;
    } else {
        emaValue = (EMA_ALPHA * median) + ((1 - EMA_ALPHA) * emaValue);
    }
}

void LoadCell::turnOn() {
    digitalWrite(_enablePin, HIGH); // Enable the HX711
}

void LoadCell::turnOff() {
    digitalWrite(_enablePin, LOW); // Disable the HX711 to save power
}