#include "LoadCell.h"

void LoadCell::begin() {
    pinMode(_enablePin, OUTPUT);
    turnOn(); // Ensure the HX711 is powered on before initialization

    scale.begin(_doutPin, _sckPin);
    scale.set_scale(calibration_factor);
    scale.tare(); // Tare the scale to zero
    Serial.println("HX711 : Initialisé avec succès");
}

void LoadCell::measureWeight() {
        // Read a reduced number of samples to avoid long blocking periods
        for (int i = 0; i < NUM_SAMPLES; i++) {
                samples[i] = scale.get_units(3); // Smaller per-read averaging for responsiveness
        }

        // Sort the samples to find the median
        std::sort(samples, samples + NUM_SAMPLES);
        float median = samples[NUM_SAMPLES / 2]; // Middle value after sorting (median)

    // Update EMA value
    if (emaValue == 0) {
        emaValue = median; // Initialize EMA with the first median value
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