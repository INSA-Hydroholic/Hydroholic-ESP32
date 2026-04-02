#ifndef _LOADCELL_H_
#define _LOADCELL_H_

#include "HX711.h"
#include <algorithm> // For std::sort

class LoadCell {
  private:
    HX711 scale;
    int _doutPin;
    int _sckPin;
    float calibration_factor;
    float samples[11];              // Array to store multiple readings for post-processing
    float emaValue;                 // Variable to store the current EMA value
    const float EMA_ALPHA = 0.1;    // Smoothing factor for EMA

  public:
    LoadCell(int doutPin, int sckPin, float calibFactor) 
        : _doutPin(doutPin), _sckPin(sckPin), calibration_factor(calibFactor) {}

    void begin();       // Initializes the HX711 and sets the calibration factor
    void measureWeight();   // Handles reading, median filtering, and EMA calculation
    float getWeight() { return emaValue; } // Returns the smoothed weight value
};

#endif // _LOADCELL_H_
