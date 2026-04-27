#include "BatteryManager.h"

void TaskBatteryManager(void * pvParameters) {
    BatteryManager* batteryManager = static_cast<BatteryManager*>(pvParameters);
    for(;;) {
        batteryManager->readBatteryLevel();
        Serial.print("Battery level : ");
        Serial.println(batteryManager->getBatteryLevel());
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Run every 500 ms
    }
}

BatteryManager::BatteryManager(int adcPin) : adcPin(adcPin) {}

void BatteryManager::begin() {
    // Configure the ADC pin for battery level reading
    pinMode(adcPin, INPUT);
    // Configure ADC resolution and attenuation
    analogReadResolution(12); // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db); // 0~3.3V range
}

void BatteryManager::readBatteryLevel() {
    for (int i = 0; i < BATTERY_NUMBER_OF_SAMPLES; i++) {
        samples[i] = analogRead(adcPin);
        delayMicroseconds(100); // Short delay so the ADC's sampling capacitor can settle between readings
    }
    
    std::sort(samples, samples + BATTERY_NUMBER_OF_SAMPLES);
    rawAdc = samples[BATTERY_NUMBER_OF_SAMPLES / 2]; // Use the median value to reduce noise
    float vNode = (rawAdc / 4095.0f) * 1.1f * 20.0f; // Convert ADC value to voltage
    batteryVoltage = vNode * 4.214f;

    // Assuming a linear discharge curve from 4.2V (100%) to 2.9V (0%), we calculate the battery level percentage. In practice, the curve is not perfectly linear, but this gives a reasonable estimate.
    float currBatteryLevel = ((batteryVoltage - 2.9f) / (4.2f - 2.9f)) * 100.0f;

    // Update EMA
    if (emaBatteryLevel == 0.0) {
        emaBatteryLevel = currBatteryLevel; // Initialize EMA with the first reading
    } else {
        emaBatteryLevel = (EMA_ALPHA * currBatteryLevel) + ((1 - EMA_ALPHA) * emaBatteryLevel);
    }

}

float BatteryManager::getBatteryLevel() const {
    return emaBatteryLevel;
}