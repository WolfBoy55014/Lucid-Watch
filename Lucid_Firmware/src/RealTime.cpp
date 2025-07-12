//
// Created by wolfboy on 6/30/25.
//

#include <Arduino.h>
#include "RealTime.h"

// Last time the RTC was calibrated (in seconds)
unsigned long lastCalibrationSeconds = 0;

// Check if year is leap year
bool isLeapYear(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Get days in a specific month
uint8_t getDaysInMonth(uint8_t month, uint16_t year) {
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days_in_month[month - 1];
}

// Normalize time components with proper carry handling
void normalizeTime(Time* time) {
    // Handle seconds carry
    if (time->second >= 60) {
        time->minute += time->second / 60;
        time->second %= 60;
    }

    // Handle minutes carry
    if (time->minute >= 60) {
        time->hour += time->minute / 60;
        time->minute %= 60;
    }

    // Handle hours carry
    if (time->hour >= 24) {
        time->day += time->hour / 24;
        time->hour %= 24;
    }

    // Handle days carry (more complex due to varying month lengths)
    while (time->day > getDaysInMonth(time->month, time->year)) {
        time->day -= getDaysInMonth(time->month, time->year);
        time->month++;

        // Handle month carry
        if (time->month > 12) {
            time->month = 1;
            time->year++;
        }
    }

    // Handle day underflow (if day becomes 0 or negative)
    while (time->day < 1) {
        time->month--;
        if (time->month < 1) {
            time->month = 12;
            time->year--;
        }
        time->day += getDaysInMonth(time->month, time->year);
    }
}

// Convert datetime to total seconds since Unix epoch (January 1, 1970)
unsigned long timeToSeconds(const Time *time) {
    // Days since Unix epoch
    uint32_t days = 0;

    // Add days for complete years
    for (uint16_t year = 1970; year < time->year; year++) {
        days += isLeapYear(year) ? 366 : 365;
    }

    // Add days for complete months in current year
    for (uint8_t month = 1; month < time->month; month++) {
        days += getDaysInMonth(month, time->year);
    }

    // Add remaining days (subtract 1 because day 1 = 0 days elapsed)
    days += time->day - 1;

    // Convert to total seconds
    uint64_t total_seconds = (uint64_t) days * 24 * 60 * 60;
    total_seconds += (uint64_t) time->hour * 60 * 60;
    total_seconds += (uint64_t) time->minute * 60;
    total_seconds += time->second;

    return total_seconds;
}

// Convert total seconds since Unix epoch back to datetime
// void secondsToTime(Time* time, unsigned long total_seconds) {
//     // Extract time components first
//     time->second = total_seconds % 60;
//     total_seconds /= 60;
//
//     time->minute = total_seconds % 60;
//     total_seconds /= 60;
//
//     time->hour = total_seconds % 24;
//     uint32_t total_days = total_seconds / 24;
//
//     // Find the year
//     time->year = 1970;
//     while (total_days >= (isLeapYear(time->year) ? 366 : 365)) {
//         total_days -= isLeapYear(time->year) ? 366 : 365;
//         time->year++;
//     }
//
//     // Find the month
//     time->month = 1;
//     while (total_days >= getDaysInMonth(time->month, time->year)) {
//         total_days -= getDaysInMonth(time->month, time->year);
//         time->month++;
//     }
//
//     // Remaining days
//     time->day = total_days + 1;
// }

void secondsToTime(Time *time, unsigned long total_seconds) {

    // Initialize the structure to zero
    memset(time, 0, sizeof(Time));

    // --- Calculate Time Components (seconds, minutes, hours) ---
    unsigned long remaining_seconds = total_seconds;

    time->second = remaining_seconds % 60;
    remaining_seconds /= 60; // Now remaining_seconds holds total minutes

    time->minute = remaining_seconds % 60;
    remaining_seconds /= 60; // Now remaining_seconds holds total hours

    time->hour = remaining_seconds % 24;
    unsigned long total_days = remaining_seconds / 24; // Now remaining_seconds holds total days

    // --- Calculate Date Components (year, month, day) ---
    int year = 1970; // Unix epoch starts January 1, 1970
    unsigned long days_since_epoch = total_days;

    // Determine the current year
    while (true) {
        int days_in_current_year = isLeapYear(year) ? 366 : 365;
        if (days_since_epoch >= days_in_current_year) {
            days_since_epoch -= days_in_current_year;
            year++;
        } else {
            break; // Found the correct year
        }
    }
    time->year = year;

    // Determine the current month and day of month
    int month = 0; // 0-indexed month (January)
    while (true) {
        int days_in_current_month = getDaysInMonth(month, year);
        if (days_since_epoch >= days_in_current_month) {
            days_since_epoch -= days_in_current_month;
            month++;
        } else {
            break; // Found the correct month
        }
    }
    time->month = month;

    // The remaining days_since_epoch is the day of the month (0-indexed)
    time->day = static_cast<int>(days_since_epoch) + 1; // Convert to 1-indexed day
}

void getRTCAsTime(RTCZero *rtc, Time *time) {
    time->second = rtc->getSeconds();
    time->minute = rtc->getMinutes();
    time->hour = rtc->getHours();
    time->day = rtc->getDay();
    time->month = rtc->getMonth();
    time->year = rtc->getYear() + 2000;
}

void getRealTime(RTCZero *rtc, Time *time) {
    getRTCAsTime(rtc, time);

    unsigned long seconds = timeToSeconds(time);

    seconds -= lastCalibrationSeconds;

    // Use double precision for the division
    double precise_seconds = static_cast<double>(seconds) * CORRECTION_FACTOR;
    seconds = lround(precise_seconds);  // Round to nearest

    seconds += lastCalibrationSeconds;

    secondsToTime(time, seconds);

    normalizeTime(time);
}

void toUnadjustedTime(Time *time) {
    unsigned long seconds = timeToSeconds(time);

    seconds -= lastCalibrationSeconds;

    // Use double precision for the division
    double precise_seconds = static_cast<double>(seconds) / CORRECTION_FACTOR;
    seconds = lround(precise_seconds);  // Round to nearest

    seconds += lastCalibrationSeconds;

    secondsToTime(time, seconds);

    normalizeTime(time);
}

void setAlarmRealTime(RTCZero *rtc, Time *time) {
    toUnadjustedTime(time);

    rtc->setAlarmTime(time->hour, time->minute, time->second);
    rtc->setAlarmDate(time->day, time->month, time->year - 2000);
}

void calibrateRTCWithTime(RTCZero *rtc, Time *time) {
    rtc->setTime(time->hour, time->minute, time->second);
    rtc->setDate(time->day, time->month, time->year - 2000);

    getRTCAsTime(rtc, time);
    lastCalibrationSeconds = timeToSeconds(time);
}

void addSeconds(Time *time, unsigned int seconds) {
    time->second += seconds;
    normalizeTime(time);
}

void addMinutes(Time *time, unsigned int minutes) {
    time->minute += minutes;
    normalizeTime(time);
}

void printTime(Time *time) {
    Serial.print(time->hour);
    Serial.print(":");
    Serial.print(time->minute);
    Serial.print(":");
    Serial.print(time->second);
    Serial.print(" ");
    Serial.print(time->month);
    Serial.print("/");
    Serial.print(time->day);
    Serial.print("/");
    Serial.println(time->year);
}

