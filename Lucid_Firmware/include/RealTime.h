//
// Created by wolfboy on 6/30/25.
//

#ifndef REALTIME_H
#define REALTIME_H

#include <RTCZero.h>

// The correction factor used to find the correct time.
// This can be calculated by dividing 32768Hz by your clock's actual frequency.
// Greater than one increases the clock speed, less than one decreases.
#define CORRECTION_FACTOR 1.02028801

extern unsigned long lastCalibrationSeconds;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} Time;

// Days in each month (non-leap year)
static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Convert datetime to total seconds since Unix epoch (January 1, 1970)
unsigned long timeToSeconds(const Time *time);

// Convert total seconds since Unix epoch back to datetime
void secondsToTime(Time* time, unsigned long total_seconds);

// Get the RTC time and store it in a `Time`
void getRTCAsTime(RTCZero *rtc, Time *time);

// Get the real time, with the correction factor applied
void getRealTime(RTCZero *rtc, Time *time);

// Take a correct time and convert it to the RTC's current time value (which is not correct)
void toUnadjustedTime(Time *time);

// Takes a correct time and sets an alarm at the incorrect time the RTC will think it will be.
// Must be redone every time the time is calibrated, or the alarm will be at the wrong time.
void setAlarmRealTime(RTCZero *rtc, Time *time);

// Reset the RTC with a correct time (most preferably from an outside source)
// It will remember when it was last calibrated so only the seconds from the last calibration until now are compensated.
void calibrateRTCWithTime(RTCZero *rtc, Time *time);

// Add any number of seconds to a `Time` object.
void addSeconds(Time *time, unsigned int seconds);

// Add any number of minutes to a `Time` object.
void addMinutes(Time *time, unsigned int minutes);

void printTime(Time *time);

#endif //REALTIME_H
