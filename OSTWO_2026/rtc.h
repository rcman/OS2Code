// File: rtc.h
// Real-Time Clock (RTC) driver header

#ifndef RTC_H
#define RTC_H

#include "types.h"

// DateTime structure (OS/2 compatible)
typedef struct {
    uint8_t  hours;        // 0-23
    uint8_t  minutes;      // 0-59
    uint8_t  seconds;      // 0-59
    uint8_t  hundredths;   // 0-99 (centiseconds)
    uint8_t  day;          // 1-31
    uint8_t  month;        // 1-12
    uint16_t year;         // Full year (e.g., 2025)
    int16_t  timezone;     // Timezone offset in minutes from UTC
    uint8_t  weekday;      // 0=Sunday, 1=Monday, ..., 6=Saturday
} DateTime;

// Initialize RTC
void rtc_init(void);

// Read current date/time from RTC
void rtc_get_datetime(DateTime* dt);

// Set RTC date/time
void rtc_set_datetime(const DateTime* dt);

#endif
