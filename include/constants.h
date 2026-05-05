#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

// Only constants that are used across multiple files (or used in main) should be defined here to maintain modularity. File or Class-specific constants should be defined in their respective header files.

#define DELAY_10_SECS   10 * 1000
#define DELAY_30_SECS   30 * 1000
#define DELAY_1_MIN     1 * 60 * 1000
#define DELAY_10_MINS   10 * 60 * 1000
#define DELAY_1_HOUR    60 * 60 * 1000

#define UPDATE_LOGS_INTERVAL 20000 // Interval for sending logs in milliseconds
#define UPDATE_BATTERY_INTERVAL 10000 // Interval for updating battery status in milliseconds

/* ============== WIFI MANAGER CONSTANTS ============== */

#define NTP_SYNC_INTERVAL 10*1000 // Interval for synchronizing time with NTP server in milliseconds
#define WIFI_CONNECTION_TIMEOUT 30000 // Timeout for WiFi connection attempts in milliseconds
#define WIFI_RETRY_DELAY 2 * 60 * 1000 // Delay before retrying to connect to WiFi in milliseconds
#define WIFI_RETRY_COUNT 5 // Number of times to retry WiFi connection before falling back to AP mode for configuration
#define WIFI_DISCONNECT_BLINK_INTERVAL 5000 // Interval between blinking sequences when WiFi connection fails in milliseconds

/* ==================================================== */
/* =============== LOAD CELL CONSTANTS ================ */

#define LOADCELL_SAVE_DATA_INTERVAL 10000    // Interval for saving load cell data in milliseconds
#define LOADCELL_MEASURE_INTERVAL 1000      // Interval for measuring weight in milliseconds

/* ==================================================== */
/* ============ BATTERY MANAGER CONSTANTS ============= */

#define BATTERY_MEASURE_INTERVAL 20000 // Interval for measuring battery status in milliseconds

/* ==================================================== */
/* ============== HMI MANAGER CONSTANTS =============== */

#define HMI_RESET_HOLD_PRESS_DURATION 10000 // Duration in milliseconds to consider a reset button press as a reset request

/* ==================================================== */
#endif
