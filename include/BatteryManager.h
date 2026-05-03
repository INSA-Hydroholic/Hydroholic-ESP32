#pragma once

#include <Arduino.h>
#include "soc/adc_channel.h"

#define BATTERY_NUMBER_OF_SAMPLES 5

void TaskBatteryManager(void * pvParameters);

class BatteryManager {
    private:
        int adcPin;
        float samples[BATTERY_NUMBER_OF_SAMPLES];
        float rawAdc;
        float batteryVoltage;
        float emaBatteryLevel = 0.0; // 0-100% battery level, smoothed with EMA
        const float EMA_ALPHA = 0.05; // Smoothing factor for EMA (0 < EMA_ALPHA <= 1, smaller is smoother but less responsive)

    public:
        BatteryManager(int adcPin);
        void begin();
        void readBatteryLevel();
        float getBatteryLevel() const;
        float getBatteryVoltage() const;
        float getRawAdc() const;
};