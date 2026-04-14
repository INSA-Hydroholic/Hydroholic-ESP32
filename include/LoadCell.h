#ifndef _LOADCELL_H_
#define _LOADCELL_H_

#include "HX711.h"
#include <algorithm> // For std::sort
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define NUM_SAMPLES 5

class LoadCell {
  private:
    HX711 scale;
    int _doutPin;
    int _sckPin;
    int _enablePin;
    float calibration_factor;
    float samples[NUM_SAMPLES];               // Array to store multiple readings for post-processing (reduced for responsiveness)
    float emaValue;                 // Variable to store the current EMA (exponential moving average) value
    bool emaInitialized;
    const float EMA_ALPHA = 0.1;    // Smoothing factor for EMA
    SemaphoreHandle_t _scaleMutex;

  public:
    LoadCell(int doutPin, int sckPin, int enablePin, float calibFactor) 
        : _doutPin(doutPin), _sckPin(sckPin), _enablePin(enablePin), calibration_factor(calibFactor), emaValue(0), emaInitialized(false), _scaleMutex(nullptr) {}

    void begin();       // Initializes the HX711 and sets the calibration factor
    void turnOn();
    void turnOff();
    void tare();
    void measureWeight();   // Handles reading, median filtering, and EMA calculation
    float getWeight() { return emaValue; } // Returns the smoothed weight value
    
};

#endif // _LOADCELL_H_
