#ifndef _LOADCELL_H_
#define _LOADCELL_H_

#include "HX711.h"

class LoadCell {
  private:
    HX711 scale;
    float calibration_factor;
    float samples[10]; // Array to store multiple readings for post-processing

  public:
    LoadCell(int doutPin, int sckPin, float calibFactor) : scale(doutPin, sckPin), calibration_factor(calibFactor) {}

    void begin();
    void getSamples(); // Collects multiple samples and stores them in the samples array
    float getMedian(); // Sorts and returns the median of the collected samples
    float getEMA(); // Calculates and returns the Exponential Moving Average of the collected samples

};

#endif // _LOADCELL_H_
