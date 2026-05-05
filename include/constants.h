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
#define NTP_SYNC_INTERVAL 10*1000 // Interval for synchronizing time with NTP server in milliseconds

#endif
