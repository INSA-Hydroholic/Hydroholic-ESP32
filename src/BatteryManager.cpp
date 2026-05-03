#include "BatteryManager.h"

void TaskBatteryManager(void * pvParameters) {
    BatteryManager* batteryManager = static_cast<BatteryManager*>(pvParameters);
    for(;;) {
        batteryManager->readBatteryLevel();
        Serial.print("Battery level : ");
        Serial.print(batteryManager->getBatteryLevel());
        Serial.print("  Battery voltage : ");
        Serial.print(batteryManager->getBatteryVoltage());
        Serial.print("  Raw ADC : ");
        Serial.println(batteryManager->getRawAdc());
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Run every 500 ms
    }
}

BatteryManager::BatteryManager(int adcPin) : adcPin(adcPin) {}

void BatteryManager::begin() {
    // Configure ADC attenuation
    analogSetAttenuation(ADC_11db); // Set attenuation for 0-3.3V range
    // Configure the ADC pin for battery level reading
    pinMode(adcPin, INPUT);
}

void BatteryManager::readBatteryLevel() {
    for (int i = 0; i < BATTERY_NUMBER_OF_SAMPLES; i++) {
        samples[i] = analogRead(adcPin);
        delayMicroseconds(100); // Short delay so the ADC's sampling capacitor can settle between readings
    }
    
    std::sort(samples, samples + BATTERY_NUMBER_OF_SAMPLES);
    rawAdc = samples[BATTERY_NUMBER_OF_SAMPLES / 2]; // Use the median value to reduce noise
    float vNode = rawAdc * (3.3f / 4095.0f); // Convert ADC reading to voltage (assuming 3.3V reference and 12-bit resolution)
    batteryVoltage = vNode * 1.37f;

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
    // Clamp the battery level to 0-100% range for safety
    if (emaBatteryLevel < 0.0f) {
        return 0.0f;
    } else if (emaBatteryLevel > 100.0f) {
        return 100.0f;
    }
    return emaBatteryLevel;
}

float BatteryManager::getBatteryVoltage() const {
    return batteryVoltage;
}

float BatteryManager::getRawAdc() const {
    return rawAdc;
}