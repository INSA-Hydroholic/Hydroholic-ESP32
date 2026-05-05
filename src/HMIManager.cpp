#include "HMIManager.h"
        
void TaskHMIManager(void * pvParameters) {
    hmi_task_parameters_t* params = (hmi_task_parameters_t*) pvParameters;
    HMIManager* hmiManager = params->hmiManager;
    unsigned long buttonPressStartTime = 0;
    for (;;) {
        // Check if the reset button is pressed for more than HMI_RESET_HOLD_PRESS_DURATION
        if (hmiManager->resetButtonPressed()) { // Assuming active LOW button
            if (buttonPressStartTime == 0) {
                buttonPressStartTime = millis();
            }
        } else {
            buttonPressStartTime = 0;
        }

        if (buttonPressStartTime != 0 && (millis() - buttonPressStartTime >= HMI_RESET_HOLD_PRESS_DURATION)) {
            hmiManager->setResetRequest();
            buttonPressStartTime = 0; // Reset the timer to avoid multiple triggers
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

void HMIManager::setResetRequest() {
    if (resetButtonSemaphore != NULL) {
        xSemaphoreTake(resetButtonSemaphore, portMAX_DELAY);
        resetRequest = true;
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
        animateLEDs(2000); // 2 seconds duration for reminder pattern
    }
}

void HMIManager::indicateError() {
    for (int i = 0; i < ERROR_REPETITIONS; i++) {
        soundBuzzer(100, 1600); // 100 ms duration, 1600 Hz frequency
        animateLEDs(333);  // 0.333 seconds duration for error pattern
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
    for (int brightness = 0; brightness <= 255 && blinkLEDs; brightness++) {
        analogWrite(ledsPin, brightness);
        delay(duration / (2 * 255));
    }
    for (int brightness = 255; brightness >= 0 && blinkLEDs; brightness--) {
        analogWrite(ledsPin, brightness);
        delay(duration / (2 * 255));
    }
}

void HMIManager::soundBuzzer(unsigned int duration, unsigned int frequency) {
    tone(buzzerPin, frequency, duration);
}

bool HMIManager::isBlinkingLEDs() const {
    return blinkLEDs;
}

void HMIManager::startBlinkingLEDs() {
    blinkLEDs = true;
}

void HMIManager::stopBlinkingLEDs() {
    blinkLEDs = false;
}

void HMIManager::setBlinkDuration(unsigned int duration) {
    blinkDuration = duration;
}

unsigned int HMIManager::getBlinkDuration() const {
    return blinkDuration;
}