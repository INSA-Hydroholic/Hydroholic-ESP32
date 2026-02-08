#ifndef CONFIG_H
#define CONFIG_H

// HX711 Pin Configuration
#define HX711_DOUT_PIN 16  // Data pin
#define HX711_SCK_PIN  4   // Clock pin

// Bluetooth Configuration
#define BT_DEVICE_NAME "Hydroholic_ESP32"

// Data Transmission Configuration
#define TRANSMISSION_INTERVAL_MS 1000  // Send data every 1 second (1000ms)

// Load Cell Calibration
#define CALIBRATION_FACTOR -7050.0  // This should be calibrated for your specific load cell
#define TARE_SAMPLES 10              // Number of samples for tare operation

#endif // CONFIG_H
