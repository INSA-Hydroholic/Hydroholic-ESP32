#include "HMIManager.h"
        
void TaskHMIManager(void * pvParameters) {
    hmi_task_parameters_t* params = (hmi_task_parameters_t*) pvParameters;
    HMIManager* hmiManager = params->hmiManager;
    unsigned long buttonPressStartTime = 0;
    for (;;) {
        // Check if the reset button is pressed for more than HMI_RESET_HOLD_PRESS_DURATION
        if (hmiManager->resetButtonPressed()) { // Assuming active LOW button
            if (buttonPressStartTime == 0) {
                Serial.println("Reset button pressed, starting timer...");
                buttonPressStartTime = millis();
            }
        } else if(buttonPressStartTime != 0) {
            Serial.println("Reset button released, resetting timer...");
            buttonPressStartTime = 0;
        }

        if (buttonPressStartTime != 0) {
            unsigned long elapsedTime = millis() - buttonPressStartTime;
            if (elapsedTime >= HMI_RESET_HOLD_PRESS_DURATION) {
                Serial.println("Reset button held for required duration, setting reset request...");
                hmiManager->setResetRequest();
                buttonPressStartTime = 0; // Reset the timer to avoid multiple triggers
            } else if (elapsedTime >= HMI_RESET_HOLD_PRESS_DURATION / 2 && !hmiManager->isBlinkingLEDs()) { // Start blinking LEDs at one third of the required duration to give feedback to the user that they are on the way to triggering a reset if they keep holding the button
                Serial.println("Reset button held for one third of required duration, starting LED indication...");
                hmiManager->setBlinkDuration(500); // Set a slower blink duration to indicate the reset is being prepared but not yet triggered
                hmiManager->startBlinkingLEDs();
            }
        }

        vTaskDelay(250 / portTICK_PERIOD_MS); // Main loop delay to reduce CPU usage, adjust as needed
    }
}

void TaskBlinkLEDs(void * pvParameters) {
    hmi_task_parameters_t* params = (hmi_task_parameters_t*) pvParameters;
    HMIManager* hmiManager = params->hmiManager;
    for (;;) {
        if (hmiManager->isBlinkingLEDs()) {
            hmiManager->animateLEDs(hmiManager->getBlinkDuration());
        } else {
            hmiManager->turnOffLEDs(); // Ensure LEDs are turned off when not blinking to avoid unintended states
            vTaskDelay(500 / portTICK_PERIOD_MS); // Delay to reduce CPU usage when not blinking
        }
        vTaskDelay(250 / portTICK_PERIOD_MS); // Delay between animations
    }
}

HMIManager::HMIManager(int resetButtonPin, int ledsPin, int buzzerPin) : resetButtonPin(resetButtonPin), ledsPin(ledsPin), buzzerPin(buzzerPin) {
    resetButtonSemaphore = xSemaphoreCreateMutex();
    hmiSemaphore = xSemaphoreCreateMutex();
}

void HMIManager::begin() {
    pinMode(resetButtonPin, INPUT_PULLUP);
    pinMode(ledsPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
}

bool HMIManager::resetButtonPressed() {
    if(digitalRead(resetButtonPin) == LOW) { // Assuming active LOW button
        delay(BUTTON_DEBOUNCE_DELAY);
        if(digitalRead(resetButtonPin) == LOW) { // Check again after debounce delay
            return true;
        }
    }
    return false;
}

void HMIManager::setResetRequest(bool request) {
    if (resetButtonSemaphore != NULL) {
        xSemaphoreTake(resetButtonSemaphore, portMAX_DELAY);
        resetRequest = request  ;
        xSemaphoreGive(resetButtonSemaphore);
    } else {
        Serial.println("Warning: resetButtonSemaphore is not initialized, cannot set reset request state.");
    }
}

bool HMIManager::getResetRequest() const {
    if (resetButtonSemaphore != NULL) {
        xSemaphoreTake(resetButtonSemaphore, portMAX_DELAY);
        bool request = resetRequest;
        xSemaphoreGive(resetButtonSemaphore);
        return request;
    } else {
        Serial.println("Warning: resetButtonSemaphore is not initialized, cannot get reset request state.");
    }
    return false; // If semaphore is not initialized, we consider that no reset has been requested to avoid potential issues
}

// soundBuzzer must be called before animateLEDs in the patterns to avoid blocking the buzzer with the LED animation delays, since the buzzer uses a non-blocking tone function that allows it to play asynchronously while the LEDs are blocking with delays for animation.
void HMIManager::remindUser() {
    for (int i = 0; i < REMINDER_REPETITIONS; i++) {
        soundBuzzer(100, 800); // 100 ms duration, 800 Hz frequency
        setBlinkDuration(2000);
        startBlinkingLEDs();
        vTaskDelay(2000 / portTICK_PERIOD_MS); // Wait for the duration of the reminder pattern before repeating
        stopBlinkingLEDs();
    }
}

void HMIManager::indicateError() {
    for (int i = 0; i < ERROR_REPETITIONS; i++) {
        soundBuzzer(100, 1600); // 100 ms duration, 1600 Hz frequency
        setBlinkDuration(333);
        startBlinkingLEDs();
        vTaskDelay(333 / portTICK_PERIOD_MS); // Wait for the duration of the error pattern before repeating
        stopBlinkingLEDs();
    }
}

void HMIManager::indicateSuccess() {
    for (int i = 0; i < SUCCESS_REPETITIONS; i++) {
        soundBuzzer(100, 800); // 100 ms duration, 800 Hz frequency
        animateLEDs(500); // 0.5 seconds duration for success pattern
    }
}

void HMIManager::animateLEDs(unsigned int duration) {
    // Oscillate the LEDs with PWM for a breathing effect
    uint32_t delayTime = (uint32_t)(duration * (1000 / 510.f)); // Total steps for a full cycle (255 up + 255 down)
    if (delayTime == 0) {
        delayTime = 1; // Ensure we don't have a zero delay
    }
    for (int brightness = 0; brightness <= 255; brightness++) {
        if (hmiSemaphore != NULL) {
            xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
            bool shouldContinue = blinkLEDs;
            xSemaphoreGive(hmiSemaphore);
            if (!shouldContinue) break;
        }
        analogWrite(ledsPin, brightness);
        delayMicroseconds(delayTime);
    }
    for (int brightness = 255; brightness >= 0; brightness--) {
        if (hmiSemaphore != NULL) {
            xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
            bool shouldContinue = blinkLEDs;
            xSemaphoreGive(hmiSemaphore);
            if (!shouldContinue) break;
        }
        analogWrite(ledsPin, brightness);
        delayMicroseconds(delayTime);
    }
}

void HMIManager::soundBuzzer(unsigned int duration, unsigned int frequency) {
    tone(buzzerPin, frequency, duration);
}

bool HMIManager::isBlinkingLEDs() const {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        bool state = blinkLEDs;
        xSemaphoreGive(hmiSemaphore);
        return state;
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot get blinking state.");
    }
    return false;
}

void HMIManager::startBlinkingLEDs() {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        blinkLEDs = true;
        xSemaphoreGive(hmiSemaphore);
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot start blinking LEDs.");
    }
}

void HMIManager::stopBlinkingLEDs() {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        blinkLEDs = false;
        xSemaphoreGive(hmiSemaphore);
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot stop blinking LEDs.");
    }
}

void HMIManager::setBlinkDuration(unsigned int duration) {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        blinkDuration = duration;
        xSemaphoreGive(hmiSemaphore);
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot set blink duration.");
    }
}

unsigned int HMIManager::getBlinkDuration() const {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        unsigned int duration = blinkDuration;
        xSemaphoreGive(hmiSemaphore);
        return duration;
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot get blink duration.");
    }
    return 0;
}

void HMIManager::turnOffLEDs() {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        blinkLEDs = false; // Stop any ongoing blinking pattern
        xSemaphoreGive(hmiSemaphore);
        int currentBrightness = analogRead(ledsPin); // Read current brightness to check if LEDs are on
        while(currentBrightness > 0) { // Gradually turn off LEDs if they are currently on
            currentBrightness -= 5; // Decrease brightness step by step for a smoother turn off effect
            if (currentBrightness < 0) {
                currentBrightness = 0; // Ensure brightness does not go below 0
            }
            analogWrite(ledsPin, currentBrightness);
            vTaskDelay(50 / portTICK_PERIOD_MS); // Delay between brightness changes for a smoother effect
        }
        analogWrite(ledsPin, 0); // Turn off LEDs
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot turn off LEDs.");
    }
}

void HMIManager::turnOffBuzzer() {
    if (hmiSemaphore != NULL) {
        xSemaphoreTake(hmiSemaphore, portMAX_DELAY);
        noTone(buzzerPin); // Turn off buzzer
        xSemaphoreGive(hmiSemaphore);
    } else {
        Serial.println("Warning: hmiSemaphore is not initialized, cannot turn off buzzer.");
    }
}
