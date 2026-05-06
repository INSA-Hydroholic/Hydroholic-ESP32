#pragma once

#include <Arduino.h>
#include "constants.h"
#include "freertos/semphr.h"

#define REMINDER_REPETITIONS 3 // Number of times to repeat the reminder pattern when reminding the user to interact with the device
#define ERROR_REPETITIONS 5 // Number of times to repeat the error pattern when indicating an error state
#define SUCCESS_REPETITIONS 2 // Number of times to repeat the success pattern when indicating a success state

#define BUTTON_DEBOUNCE_DELAY 50 // Delay in milliseconds to debounce the reset button input

void TaskHMIManager(void * pvParameters);  // Task function for handling reset button input and indicating that to the main loop.

void TaskBlinkLEDs(void * pvParameters); // Task function for handling LED animations without blocking the main loop or other tasks that might want to use the LEDs for indications.

class HMIManager {
    private:
        int resetButtonPin;
        int ledsPin;
        int buzzerPin;
        bool blinkLEDs = false;
        unsigned int blinkDuration = 2000; // Default duration for blinking pattern in milliseconds
        bool resetRequest = false;
        SemaphoreHandle_t resetButtonSemaphore; // Semaphore to protect access to resetRequest variable
        SemaphoreHandle_t hmiSemaphore; // Semaphore to protect access to HMI outputs (LEDs and buzzer) to avoid conflicts between different tasks trying to use them at the same time

    public:
        HMIManager(int resetButtonPin, int ledsPin, int buzzerPin);
        void begin();
        bool resetButtonPressed();      // Checks if the reset button is currently being pressed (regardless of duration)
        void setResetRequest(bool request = true);         // Sets the reset request flag
        bool getResetRequest() const;    // True if the reset button has been pressed for more than HMI_RESET_HOLD_PRESS_DURATION
        void remindUser();              // Runs LED and buzzer patterns to remind the user to interact with the device (e.g. to drink water)
        void animateLEDs(unsigned int duration = 2000);
        void soundBuzzer(unsigned int duration = 100, unsigned int frequency = 1000);  // Duration for a beep sound in milliseconds
        void indicateError();           // Runs LED and buzzer patterns to indicate an error state (e.g. failed WiFi connection)
        void indicateSuccess();         // Runs LED and buzzer patterns to indicate a success state (e.g. successful WiFi connection)
        bool isBlinkingLEDs() const;
        void startBlinkingLEDs();
        void stopBlinkingLEDs();
        void setBlinkDuration(unsigned int duration);
        unsigned int getBlinkDuration() const;
        void turnOffLEDs(); // Immediately turns off the LEDs, can be used to interrupt any ongoing blinking pattern
        void turnOffBuzzer(); // Immediately turns off the buzzer, can be used to interrupt any ongoing buzzing pattern
};

struct hmi_task_parameters_t {
    HMIManager* hmiManager;
    // Add other parameters if needed in the future
};