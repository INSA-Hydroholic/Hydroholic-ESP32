#pragma once

#include "HX711.h"
#include <algorithm> // For std::sort
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <RTClib.h>
#include "Storage.h"

#define LOADCELL_NUM_SAMPLES 5
#define STABILITY_SAMPLES 10
#define STABILITY_THRESHOLD 1.0 // Threshold for considering the weight stable (in grams)
#define LOADCELL_DATA_FILE "/raw_weight.csv"  // File to log raw weight with timestamps
#define SAVE_DATA_INTERVAL 5000

void TaskLoadCell(void * pvParameters);  // Expects a pointer to a LoadCell instance as parameter


class LoadCell {
  private:
    HX711 scale;
    int _doutPin;
    int _sckPin;
    int _enablePin;
    bool isStable = false;
    int historyIndex = 0;           // Index of the next position to write in the history buffer
    float history[STABILITY_SAMPLES];              // Circular buffer to store recent weight values for stability analysis
    int historyCount = 0;           // Number of valid entries currently present in history
    float calibration_factor;
    float samples[LOADCELL_NUM_SAMPLES];     // Array to store multiple readings for post-processing (reduced for responsiveness)
    float emaValue;                 // Variable to store the current EMA (exponential moving average) value
    bool emaInitialized;
    const float EMA_ALPHA = 0.5;    // Smoothing factor for EMA (0 < EMA_ALPHA <= 1, smaller is smoother but less responsive)
    SemaphoreHandle_t _scaleMutex = nullptr;  // Mutex to protect access to EMA and stability state during tare/calibration updates

    void resetStabilityState();

  public:
    LoadCell(int doutPin, int sckPin, float calibFactor, int enablePin = -1) 
        : _doutPin(doutPin), _sckPin(sckPin), _enablePin(enablePin), calibration_factor(calibFactor), emaValue(0), emaInitialized(false), _scaleMutex(nullptr) {}

    void begin();       // Initializes the HX711 and sets the calibration factor
    void turnOn();
    void turnOff();
    void tare();
    bool setCalibrationFactor(float factor);
    float getCalibrationFactor();
    void measureWeight();   // Handles reading, median filtering, and EMA calculation
    float getWeight() const; // Returns the smoothed weight value
    bool isStableWeight() const; // Returns whether the weight is currently considered stable
};

struct loadcell_task_parameters_t {
    LoadCell* loadCell;
    Storage* storage;
    RTC_DS1307* rtc;
};