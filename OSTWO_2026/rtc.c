// File: rtc.c
// Real-Time Clock (RTC) driver implementation
// Reads/writes CMOS RTC chip via ports 0x70 and 0x71

#include "types.h"
#include "rtc.h"

// External I/O functions
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern void printf(const char* format, ...);

// CMOS/RTC ports
#define CMOS_ADDRESS    0x70
#define CMOS_DATA       0x71

// CMOS RTC registers
#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_WEEKDAY     0x06
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_CENTURY     0x32  // Some RTCs have a century register
#define RTC_STATUS_A    0x0A
#define RTC_STATUS_B    0x0B

// Read a byte from CMOS
static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// Write a byte to CMOS
static void cmos_write(uint8_t reg, uint8_t value) {
    outb(CMOS_ADDRESS, reg);
    outb(CMOS_DATA, value);
}

// Convert BCD to binary (if needed)
static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Convert binary to BCD (if needed)
static uint8_t binary_to_bcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}

// Check if RTC update is in progress
static int rtc_update_in_progress(void) {
    outb(CMOS_ADDRESS, RTC_STATUS_A);
    return (inb(CMOS_DATA) & 0x80) != 0;
}

// Initialize RTC
void rtc_init(void) {
    // Nothing special needed for initialization
    // RTC is battery-backed and always running
}

// Read current date/time from RTC
void rtc_get_datetime(DateTime* dt) {
    uint8_t century = 0;
    uint8_t last_second;
    uint8_t last_minute;
    uint8_t last_hour;
    uint8_t last_day;
    uint8_t last_month;
    uint8_t last_year;
    uint8_t last_century;
    uint8_t registerB;

    // Wait until RTC update is not in progress
    while (rtc_update_in_progress());

    // Read RTC values (do it twice to ensure consistency)
    // This is necessary because RTC values can change mid-read
    do {
        last_second = cmos_read(RTC_SECONDS);
        last_minute = cmos_read(RTC_MINUTES);
        last_hour = cmos_read(RTC_HOURS);
        last_day = cmos_read(RTC_DAY);
        last_month = cmos_read(RTC_MONTH);
        last_year = cmos_read(RTC_YEAR);
        last_century = cmos_read(RTC_CENTURY);  // May not be supported

        // Wait for update to complete
        while (rtc_update_in_progress());

        // Re-read and check if values changed
        dt->seconds = cmos_read(RTC_SECONDS);
        dt->minutes = cmos_read(RTC_MINUTES);
        dt->hours = cmos_read(RTC_HOURS);
        dt->day = cmos_read(RTC_DAY);
        dt->month = cmos_read(RTC_MONTH);
        dt->year = cmos_read(RTC_YEAR);
        century = cmos_read(RTC_CENTURY);
        dt->weekday = cmos_read(RTC_WEEKDAY);
    } while ((last_second != dt->seconds) ||
             (last_minute != dt->minutes) ||
             (last_hour != dt->hours) ||
             (last_day != dt->day) ||
             (last_month != dt->month) ||
             (last_year != dt->year) ||
             (last_century != century));

    // Read Status Register B to check format
    registerB = cmos_read(RTC_STATUS_B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        dt->seconds = bcd_to_binary(dt->seconds);
        dt->minutes = bcd_to_binary(dt->minutes);
        dt->hours = ((dt->hours & 0x0F) + (((dt->hours & 0x70) / 16) * 10)) | (dt->hours & 0x80);
        dt->day = bcd_to_binary(dt->day);
        dt->month = bcd_to_binary(dt->month);
        dt->year = bcd_to_binary(dt->year);
        if (century != 0) {
            century = bcd_to_binary(century);
        }
    }

    // Convert 12-hour to 24-hour if necessary
    if (!(registerB & 0x02) && (dt->hours & 0x80)) {
        dt->hours = ((dt->hours & 0x7F) + 12) % 24;
    }

    // Calculate full year
    // If century register exists and is non-zero, use it
    if (century != 0) {
        dt->year = (uint16_t)century * 100 + dt->year;
    } else {
        // Assume 21st century for years 00-99
        dt->year += 2000;
    }

    // Hundredths and timezone not provided by RTC
    dt->hundredths = 0;
    dt->timezone = 0;  // UTC
}

// Set RTC date/time
void rtc_set_datetime(const DateTime* dt) {
    uint8_t registerB;
    uint8_t year, century;

    // Calculate year and century
    year = dt->year % 100;
    century = dt->year / 100;

    // Wait until RTC update is not in progress
    while (rtc_update_in_progress());

    // Read Status Register B
    registerB = cmos_read(RTC_STATUS_B);

    // Disable NMI and RTC updates
    outb(CMOS_ADDRESS, RTC_STATUS_B);
    outb(CMOS_DATA, registerB | 0x80);

    // Convert to BCD if necessary
    uint8_t sec = dt->seconds;
    uint8_t min = dt->minutes;
    uint8_t hr = dt->hours;
    uint8_t dy = dt->day;
    uint8_t mon = dt->month;
    uint8_t yr = year;
    uint8_t cent = century;

    if (!(registerB & 0x04)) {
        sec = binary_to_bcd(sec);
        min = binary_to_bcd(min);
        hr = binary_to_bcd(hr);
        dy = binary_to_bcd(dy);
        mon = binary_to_bcd(mon);
        yr = binary_to_bcd(yr);
        cent = binary_to_bcd(cent);
    }

    // Write values to RTC
    cmos_write(RTC_SECONDS, sec);
    cmos_write(RTC_MINUTES, min);
    cmos_write(RTC_HOURS, hr);
    cmos_write(RTC_DAY, dy);
    cmos_write(RTC_MONTH, mon);
    cmos_write(RTC_YEAR, yr);
    cmos_write(RTC_CENTURY, cent);
    cmos_write(RTC_WEEKDAY, dt->weekday);

    // Re-enable NMI and RTC updates
    outb(CMOS_ADDRESS, RTC_STATUS_B);
    outb(CMOS_DATA, registerB);
}
