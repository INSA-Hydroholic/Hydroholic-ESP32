/**
 * @file main.cpp
 * @brief ESP32 Load Cell (HX711) with Bluetooth Data Transmission
 * 
 * This program reads weight data from a load cell using the HX711 module
 * and transmits the data via Bluetooth Classic (SPP) at regular intervals.
 * 
 * Hardware Requirements:
 * - ESP32 Development Board
 * - HX711 Load Cell Amplifier
 * - Load Cell
 * 
 * Pin Connections:
 * - HX711 DOUT -> ESP32 GPIO 16
 * - HX711 SCK  -> ESP32 GPIO 4
 * - HX711 VCC  -> ESP32 3.3V or 5V
 * - HX711 GND  -> ESP32 GND
 */

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <HX711.h>
#include "config.h"

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Global objects
HX711 scale;
BluetoothSerial SerialBT;

// Variables for timing
unsigned long lastTransmissionTime = 0;

// System status
bool isCalibrated = false;
bool isConnected = false;

/**
 * @brief Initialize the HX711 load cell
 */
void initLoadCell() {
  Serial.println("Initializing HX711 Load Cell...");
  
  // Initialize HX711 with the defined pins
  scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  
  // Check if HX711 is ready
  if (scale.is_ready()) {
    Serial.println("HX711 initialized successfully");
    
    // Set the calibration factor
    scale.set_scale(CALIBRATION_FACTOR);
    
    // Tare the scale (reset to zero)
    Serial.println("Taring the scale...");
    scale.tare(TARE_SAMPLES);
    
    isCalibrated = true;
    Serial.println("Load cell ready!");
  } else {
    Serial.println("ERROR: HX711 not found. Check wiring!");
    isCalibrated = false;
  }
}

/**
 * @brief Initialize Bluetooth
 */
void initBluetooth() {
  Serial.println("Initializing Bluetooth...");
  
  // Initialize Bluetooth with device name
  if (!SerialBT.begin(BT_DEVICE_NAME)) {
    Serial.println("ERROR: Bluetooth initialization failed!");
    return;
  }
  
  Serial.print("Bluetooth initialized. Device name: ");
  Serial.println(BT_DEVICE_NAME);
  Serial.println("Waiting for Bluetooth connection...");
}

/**
 * @brief Read weight from load cell and return as float
 * @return Current weight reading in grams (or configured unit)
 */
float readWeight() {
  if (!scale.is_ready()) {
    Serial.println("WARNING: HX711 not ready");
    return 0.0;
  }
  
  // Get weight reading (average of multiple readings for stability)
  float weight = scale.get_units(10);
  return weight;
}

/**
 * @brief Send weight data via Bluetooth
 * @param weight The weight value to transmit
 */
void sendWeightData(float weight) {
  // Create JSON-formatted data for better parsing on phone app
  String jsonData = "{";
  jsonData += "\"weight\":" + String(weight, 2) + ",";
  jsonData += "\"unit\":\"g\",";
  jsonData += "\"timestamp\":" + String(millis()) + ",";
  jsonData += "\"device\":\"" + String(BT_DEVICE_NAME) + "\"";
  jsonData += "}\n";
  
  // Send via Bluetooth
  if (SerialBT.hasClient()) {
    SerialBT.print(jsonData);
    
    // Also print to Serial for debugging
    Serial.print("Sent: ");
    Serial.print(jsonData);
  } else if (isConnected) {
    // Connection lost
    isConnected = false;
    Serial.println("Bluetooth client disconnected");
  }
}

/**
 * @brief Arduino setup function
 */
void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("Hydroholic ESP32 - Load Cell System");
  Serial.println("=================================\n");
  
  // Initialize HX711 Load Cell
  initLoadCell();
  
  // Initialize Bluetooth
  initBluetooth();
  
  Serial.println("\nSystem ready!");
  Serial.println("Connect via Bluetooth to receive data");
  Serial.println("---------------------------------\n");
}

/**
 * @brief Arduino main loop function
 */
void loop() {
  // Check Bluetooth connection status
  if (SerialBT.hasClient() && !isConnected) {
    isConnected = true;
    Serial.println("Bluetooth client connected!");
    SerialBT.println("Connected to Hydroholic ESP32");
    SerialBT.println("Receiving weight data...");
  }
  
  // Check if it's time to send data
  unsigned long currentTime = millis();
  if (currentTime - lastTransmissionTime >= TRANSMISSION_INTERVAL_MS) {
    lastTransmissionTime = currentTime;
    
    // Read weight from load cell
    if (isCalibrated && scale.is_ready()) {
      float weight = readWeight();
      
      // Display on Serial Monitor
      Serial.print("Weight: ");
      Serial.print(weight, 2);
      Serial.println(" g");
      
      // Send via Bluetooth if connected
      if (SerialBT.hasClient()) {
        sendWeightData(weight);
      }
    } else if (isCalibrated) {
      Serial.println("WARNING: HX711 not responding");
    }
  }
  
  // Handle incoming Bluetooth commands (optional feature)
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();
    
    // Command: TARE - Reset scale to zero
    if (command.equalsIgnoreCase("TARE")) {
      Serial.println("Tare command received");
      SerialBT.println("Taring scale...");
      scale.tare(TARE_SAMPLES);
      SerialBT.println("Scale tared successfully");
    }
    // Command: CAL:value - Set calibration factor
    else if (command.startsWith("CAL:")) {
      float newFactor = command.substring(4).toFloat();
      scale.set_scale(newFactor);
      Serial.print("Calibration factor set to: ");
      Serial.println(newFactor);
      SerialBT.print("Calibration factor updated: ");
      SerialBT.println(newFactor);
    }
    // Command: STATUS - Get system status
    else if (command.equalsIgnoreCase("STATUS")) {
      SerialBT.println("System Status:");
      SerialBT.print("  Load Cell: ");
      SerialBT.println(isCalibrated ? "OK" : "ERROR");
      SerialBT.print("  Bluetooth: Connected");
      SerialBT.print("  Interval: ");
      SerialBT.print(TRANSMISSION_INTERVAL_MS);
      SerialBT.println(" ms");
    }
    // Command: HELP - Show available commands
    else if (command.equalsIgnoreCase("HELP")) {
      SerialBT.println("Available Commands:");
      SerialBT.println("  TARE         - Reset scale to zero");
      SerialBT.println("  CAL:value    - Set calibration factor");
      SerialBT.println("  STATUS       - Get system status");
      SerialBT.println("  HELP         - Show this help");
    }
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}
