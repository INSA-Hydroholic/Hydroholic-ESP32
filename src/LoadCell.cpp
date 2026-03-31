#include "LoadCell.h"

void LoadCell::begin() {
    scale.begin(_doutPin, _sckPin);
    scale.set_scale(calibration_factor);
    scale.tare(); // Tare the scale to zero
    Serial.println("HX711 : Initialisé avec succès");
}

void LoadCell::measureWeight() {
    // Read multiple samples for median filtering
    for (int i = 0; i < 11; i++) {
        samples[i] = scale.get_units(10); // Average of 10 readings for stability
      }

    // Sort the samples to find the median
    std::sort(samples, samples + 11);
      float median = samples[5]; // The middle value after sorting

    // Update EMA value
    if (emaValue == 0) {
        emaValue = median; // Initialize EMA with the first median value
    } else {
        emaValue = (EMA_ALPHA * median) + ((1 - EMA_ALPHA) * emaValue);
    }
}