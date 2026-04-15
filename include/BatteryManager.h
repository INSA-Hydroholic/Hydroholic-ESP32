#pragma once

#include <Arduino.h>
#include "soc/adc_channel.h"

#define BATTERY_NUMBER_OF_SAMPLES 5

class BatteryManager {
    private:
        int adcPin;
        float samples[BATTERY_NUMBER_OF_SAMPLES];
        float rawAdc;
        float batteryVoltage;
        float emaBatteryLevel = 0.0;
        const float EMA_ALPHA = 0.2; // Smoothing factor for EMA (0 < EMA_ALPHA <= 1, smaller is smoother but less responsive)

    public:
        BatteryManager(int adcPin);
        void begin();
        void readBatteryLevel();
        float getBatteryLevel() const;
};