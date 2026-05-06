#include <Arduino.h>
#include "LoadCell.h"
#include "BLEManager.h"
#include "WiFiManager.h"
#include "BatteryManager.h"
#include "HMIManager.h"
#include "Storage.h"
#include "constants.h"
#include <RTCLib.h>
#include <Wire.h>  // For I2C connections
#include <time.h>
#include "webpage.h"
#include "urlDecode.h"

#define HX711_DOUT_PIN      17
#define HX711_SCK_PIN       16
#define LED_PIN             19
#define BATTERY_ADC_PIN     34
#define BUZZER_PIN          23
#define RST_BUTTON_PIN      18

#define CONFIG_FILE "/config.csv"

BLEManager* bleManager;
WiFiManager* wifiManager;
Storage dataStorage("/data.csv");
LoadCell loadCell(HX711_DOUT_PIN, HX711_SCK_PIN, 1000.0);
BatteryManager batteryManager(BATTERY_ADC_PIN);
RTC_DS1307 rtc;
HMIManager hmiManager(RST_BUTTON_PIN, LED_PIN, BUZZER_PIN);

volatile bool isStorageReady = false;
volatile bool isTimeSynched = false;
static unsigned long lastUpdateServerTime = 0;
static unsigned long lastGetReminderTime = 0;
static unsigned long lastSaveDataTime = 0;

enum STATE {
    WIFI = 0,
    BLE,
    SETUP
};

STATE currentState = SETUP; // Start in WiFi for debugging, MUST be changed to SETUP for production to allow initial configuration

#define NUM_TASKS 5
TaskHandle_t tasksHandles[NUM_TASKS]; // Array to hold task handles for cleanup and suspending when switching states

// Config buffers with proper allocation
char confSSID[256];
char confPassword[256];
char confApiURL[256];
char confOrgCode[16];

void setup() {
    Serial.begin(115200);
    Wire.begin();  // TODO : check for power optimisations by turning off maybe
    if (!rtc.begin()) {
        Serial.println("ERROR : Couldn't communicate with the RTC DS1307");
    }

    // Retrieve rtc time, if it's later than the compile time, it means the rtc has a valid time (either from previous sync or from battery backup) and we can use it. Otherwise, we set it to the compile time as a fallback
    if (rtc.now().unixtime() > DateTime(F(__DATE__),F(__TIME__)).unixtime()) {
        Serial.println("RTC time is valid, using it.");
    } else {
        Serial.println("RTC time is not valid, setting it to compile time.");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Initialize task handles to NULL
    for (int i = 0; i < NUM_TASKS; i++) {
        tasksHandles[i] = NULL;
    }

    if (dataStorage.begin()) {
        isStorageReady = true;
    } else {
        Serial.println("ERROR : Couldn't start LittleFS data storage");
        ESP.restart();  // Restart to attempt recovery from partitioning
        // TODO : verify if it should continue or halt here
    }

    pinMode(2, OUTPUT); // Builtin LED for status indication
    loadCell.begin();
    batteryManager.begin();
    hmiManager.begin();

    // Flash LEDs twice at startup to indicate the device has booted successfully and is starting the tasks
    hmiManager.startBlinkingLEDs();
    hmiManager.animateLEDs(500);
    delay(300);
    hmiManager.animateLEDs(500);
    delay(300);
    hmiManager.stopBlinkingLEDs();

    String deviceID = "0";
    // Search for existing configuration in config.csv
    File configFile = LittleFS.open("/config.csv", "r");
    if (configFile) {
        // Iterate through lines to retrieve configuration values
        while (configFile.available()) {
            String line = configFile.readStringUntil('\n');
            String key = line.substring(0, line.indexOf(':'));
            String value = line.substring(line.indexOf(':') + 1);
            Serial.println("Config line - Key: " + key + ", Value: " + value);
            if (key == "STATE") {
                if (value.startsWith("wifi")) {
                    currentState = WIFI;
                } else if (value.startsWith("ble")) {
                    currentState = BLE;
                } else {
                    currentState = SETUP;
                }
                Serial.print("Operation mode loaded from config: ");
                Serial.println(currentState);
            } else if (key == "CALIB_FACTOR") {
                float factor = value.toFloat();
                if (factor > 0) {
                    loadCell.setCalibrationFactor(factor);
                    Serial.print("Calibration factor loaded from config: ");
                    Serial.println(factor, 6);
                } else {
                    Serial.println("Invalid calibration factor in config.csv, using default.");
                }
            } else if (key == "SSID") {
                strncpy(confSSID, value.c_str(), sizeof(confSSID) - 1);
                confSSID[sizeof(confSSID) - 1] = '\0';
                Serial.print("SSID loaded from config: ");
                Serial.println(confSSID);
            } else if (key == "PASSWORD") {
                strncpy(confPassword, value.c_str(), sizeof(confPassword) - 1);
                confPassword[sizeof(confPassword) - 1] = '\0';
                Serial.print("Password loaded from config: ");
                Serial.println(confPassword);
            } else if (key == "API_URL") {
                strncpy(confApiURL, value.c_str(), sizeof(confApiURL) - 1);
                confApiURL[sizeof(confApiURL) - 1] = '\0';
                Serial.print("API URL loaded from config: ");
                Serial.println(confApiURL);
            } else if (key == "ORG_CODE") {
                strncpy(confOrgCode, value.c_str(), sizeof(confOrgCode) - 1);
                confOrgCode[sizeof(confOrgCode) - 1] = '\0';
                Serial.print("Organization Code loaded from config: ");
                Serial.println(confOrgCode);
            }
        }
    } else {
        Serial.println("Could not open config.csv for reading.");
    }

    // Run the sensor task on core 1 so it can't starve the core-0 Idle/Watchdog
    loadcell_task_parameters_t loadCellParams{&loadCell, &dataStorage, &rtc};
    xTaskCreatePinnedToCore(TaskLoadCell, "TaskLoadCell", 10000, &loadCellParams, 1, &tasksHandles[0], 1);
    xTaskCreatePinnedToCore(TaskBatteryManager, "TaskBatteryManager", 10000, &batteryManager, 1, &tasksHandles[1], 1);
    hmi_task_parameters_t hmiParams{&hmiManager};
    xTaskCreatePinnedToCore(TaskHMIManager, "TaskHMIManager", 10000, &hmiParams, 1, &tasksHandles[2], 1);
    xTaskCreatePinnedToCore(TaskBlinkLEDs, "TaskBlinkLEDs", 10000, &hmiParams, 1, &tasksHandles[3], 1);

    // Run WiFi and BLE tasks on core 0, since drivers already run there anyways.
    if (currentState == STATE::WIFI) {
        Serial.println("Running in WIFI mode.");
        wifiManager = new WiFiManager(&rtc, deviceID);
        wifiManager->begin(confSSID, confPassword, opmode::NORMAL);  // Defined in environment.ini
        wifiManager->setAPIURL(confApiURL);
        wifiManager->setOrgCode(confOrgCode);
        xTaskCreatePinnedToCore(TaskWiFiManager, "TaskWiFiManager", 10000, wifiManager, 1, &tasksHandles[4], 0);
    } else if (currentState == STATE::SETUP) {
        Serial.println("Running in SETUP mode. Starting WiFi AP for configuration.");
        wifiManager = new WiFiManager(&rtc, deviceID);
        wifiManager->begin(NULL, NULL, opmode::CONFIGURATION);
        xTaskCreatePinnedToCore(TaskWiFiManager, "TaskWiFiManager", 10000, wifiManager, 1, &tasksHandles[4], 0);
    } else if (currentState == STATE::BLE) {
        Serial.println("Running in BLE mode.");
        bleManager = new BLEManager("Hydroholic");
        // Create a structure to hold the parameters for the BLE task, since we can only pass a single void* parameter to the task function
        ble_task_parameters_t bleParams{bleManager, &dataStorage, &loadCell, &batteryManager};
        xTaskCreatePinnedToCore(TaskBLEManager, "TaskBLEManager", 10000, &bleParams, 1, &tasksHandles[4], 0);
    }
}

// The main loop will handle the orchestration of services
void loop() {
    // RESET Request handling
    // TODO : clear storage and safely reset the microcontroller
    // Start blinking LEDs to confirm the button press has been registered and a reset request is being processed while waiting for the reset duration to be reached in the TaskHMIManager
    if (hmiManager.getResetRequest()) {
        // Suspend other tasks to ensure the reset process is not interrupted
        for (int i = 0; i < NUM_TASKS; i++) {
            if (tasksHandles[i] != NULL) {
                vTaskSuspend(tasksHandles[i]);
            }
        }
        Serial.println("Reset requested...");
        ledcDetachPin(LED_PIN);  // Disable any PWM that might be active on the pin
        for (int i = 0; i < 10; i++) { // Blink LEDs for 5 seconds to indicate reset request has been registered
            digitalWrite(LED_PIN, HIGH);
            delay(200);
            digitalWrite(LED_PIN, LOW);
            delay(200);
        }
        dataStorage.clear(LOADCELL_DATA_FILE);
        Serial.println("Restarting device...");
        ESP.restart();
    }

    if (currentState == STATE::SETUP) {
        // In setup mode, so here we suspend other tasks to ensure they don't interfere with the setup process and consume resources while the user is configuring the device through the WiFi AP. Once the user finishes the configuration and the device restarts
        for (int i = 0; i < NUM_TASKS; i++) {
            if (tasksHandles[i] != NULL) {
                vTaskSuspend(tasksHandles[i]);
            }
        }
        // Initialize Web server 
        WiFiServer server(80);
        server.begin();

        for (;;) {
            WiFiClient client = server.available();
            if (client) {
                Serial.println("Client connected");
                while (client.connected() && !client.available()) {
                    delay(10);
                }

                if (client.available()) {
                    // The first line of the HTTP request contains the method and endpoint, e.g. "GET /setup HTTP/1.1"
                    String request = client.readStringUntil('\r');
                    Serial.println("Received request: " + request);
                    client.readStringUntil('\n'); // Consume the trailing '\n'

                    if (request.indexOf("GET /setup") >= 0) {
                        Serial.println("Client requested setup "); 
                        // Flush the remaining HTTP headers from the client
                        while (client.available()) {
                            String line = client.readStringUntil('\n');
                            if (line == "\r") { 
                                break; // An empty line indicates the end of the headers
                            }
                        }

                        // Send the HTTP response headers
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type: text/html");
                        client.println("Connection: close");
                        client.println(); // Crucial: blank line separates headers from body

                        // Send the payload
                        client.println(index_html); 
                        
                    } else if (request.indexOf("POST /save") >= 0) {
                        // Read the POST data from the client (the new configuration)
                        String postData = "";
                        while (client.available()) {
                            postData += client.readStringUntil('\n');
                        }
                        Serial.println("Received POST data: " + postData);
                        String opMode = urlDecode(postData.substring(postData.indexOf("mode=") + 5, postData.indexOf("&ssid=")));
                        String ssid = urlDecode(postData.substring(postData.indexOf("ssid=") + 5, postData.indexOf("&password=")));
                        String password = urlDecode(postData.substring(postData.indexOf("password=") + 9, postData.indexOf("&org_code=")));
                        String organizationCode = urlDecode(postData.substring(postData.indexOf("org_code=") + 9, postData.indexOf("org_code=") + 15));  // 6 digits code
                        int apiURLStartIndex = postData.indexOf("api_url=") + 8;
                        String apiURL = urlDecode(postData.substring(apiURLStartIndex, postData.indexOf("&", apiURLStartIndex)));

                        Serial.println("Parsed config - Mode: " + opMode + ", SSID: " + ssid + ", Password: " + password + ", API URL: " + apiURL + ", Org Code: " + organizationCode);

                        // Validate input data
                        if ((opMode != "wifi" && opMode != "ble") || ssid.length() == 0 || organizationCode.length() != 6) {
                            Serial.println("Invalid configuration data received.");
                            client.println("HTTP/1.1 400 Bad Request");
                            client.println("Connection: close");
                            client.println();
                            client.println("Invalid configuration data. Please check your input and try again.");
                            client.stop();
                            return;
                        }

                        // Save the configuration to config.csv in the format "KEY:VALUE"
                        dataStorage.clear(CONFIG_FILE); // Clear existing config

                        File configFile = LittleFS.open(CONFIG_FILE, "w");
                        if (configFile) {
                            configFile.println("STATE:" + opMode);
                            configFile.println("SSID:" + ssid);
                            configFile.println("PASSWORD:" + password);
                            configFile.println("API_URL:" + apiURL);
                            configFile.println("ORG_CODE:" + organizationCode);
                            configFile.close();
                            Serial.println("Configuration saved to " CONFIG_FILE);
                        } else {
                            Serial.println("Failed to open " CONFIG_FILE " for writing.");
                        }

                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type: text/plain");
                        client.println("Connection: close");
                        client.println();
                        client.println("Settings saved. Hydrobase is rebooting...");
                        ESP.restart();
                    } else {  // Handle unknown endpoints with a 404 response
                        client.println("HTTP/1.1 404 Not Found");
                        client.println("Connection: close");
                        client.println();
                    }
                }
                
                // Give the web browser time to receive the data
                delay(10);
                
                // 5. Close the connection
                client.stop();
                Serial.println("Client disconnected.");
            }
        }
    } else if (currentState == STATE::BLE) {
        // The BLEManager task handles everything, so we just run an infinite loop
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } else if (currentState == STATE::WIFI) {
        unsigned long currentTime = millis();
        // Check if user needs to be reminded to interact with the device every GET_REMINDER_INTERVAL seconds (e.g. to drink water, etc)
        if (currentTime - lastGetReminderTime >= GET_REMINDER_INTERVAL) {
            lastGetReminderTime = currentTime;
            Serial.println("Checking if user needs to be reminded...");
            String response = "";
            String &responseRef = response; // Need a non-const reference to pass to the getData function
            if(wifiManager->getData("/device/:ID/user?"+wifiManager->getDeviceID(), responseRef, "text/plain")) {
                Serial.println("Received associated user ID from server: " + response);
                if (response != "-1") {
                    String userID = response; // Store user ID for future use if needed
                    response = ""; // Clear response string to reuse it for the next request
                    String &responseRef = response; // Update reference to the cleared response string
                    if (wifiManager->getData("/users/"+userID+"/alerts?evaluated_time=1", responseRef, "text/plain")) {
                        Serial.println("Received reminder response from server: " + response);
                        if (response.substring(14, 18) == "true") { // If the server indicates the user needs to be reminded, trigger the reminder pattern on the device
                            hmiManager.remindUser();
                        }
                    } else {
                        Serial.println("Error checking reminder status from server.");
                    }
                }
            } else {
                Serial.println("Error checking reminder status from server.");
            }
        }
        // Send data to the server every UPDATE_LOGS_INTERVAL seconds
        if (currentTime - lastUpdateServerTime >= UPDATE_LOGS_INTERVAL) {
            lastUpdateServerTime = currentTime;
            
            // Get current RTC time for timestamp synchronization with the server
            DateTime now = rtc.now();
            uint32_t epochTime = now.unixtime();
            Serial.print("Current RTC time: ");
            Serial.println(now.timestamp());
            // TODO : retrieve NTP time and compare with RTC time to check if it's synched, if not, sync it and update stored timestamps with the new time. For now, we will assume it's synched

            // Send loadcell logs to the server
            String hydration_logs = dataStorage.readContent(LOADCELL_DATA_FILE);  // TODO : optimize this by reading in chunks instead of the whole file at once
            if (hydration_logs.length() > 0) {
                Serial.println("Sending " + String(hydration_logs.length()) + " bytes of hydration logs to server:\n" + hydration_logs);
                if(wifiManager->sendData("/device/:ID/logs", hydration_logs, "text/csv")) {
                    Serial.println("Data sent successfully.");
                    dataStorage.clear(LOADCELL_DATA_FILE); // Clear the stored data after successful sync
                } else {
                    Serial.println("Error sending logs to server.");
                    // TODO : handle failed data sync (retry mechanism, etc)
                }
            }

            // Send battery status to the server
            float batteryLevel = batteryManager.getBatteryLevel();
            String payload = "{\"epoch\":" + String(epochTime) + ", \"battery\":" + String(batteryLevel, 2) + "}";
            Serial.println("Sending data to server: " + payload);
            wifiManager->sendData("/device/:ID/status", payload, "application/json");
        }
    }

    vTaskDelay(250 / portTICK_PERIOD_MS); // Main loop runs every 250 ms
}