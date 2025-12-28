#include <Arduino.h>
#include <RTCZero.h>
#include <RealTime.h>
#include <Wire.h>
#include <Adafruit_FreeTouch.h>

#define DELAY(value) delay(value / 6)

#define WBTB_TIMER
#define POLL_TIME 5 // only used when WBTB_TIMER is defined (in seconds)
#define TOUCH_THRESH_HOLD 600

// #define TESTING_THRESH_HOLD // define to run thresh hold testing loop
                            // must have WBTB_TIMER defined also

RTCZero rtc;
#ifdef WBTB_TIMER
Adafruit_FreeTouch qt = Adafruit_FreeTouch(A2, OVERSAMPLE_4, RESISTOR_50K, FREQ_MODE_NONE);
#endif

void i2cReceiveTime(int numBytes) {
    if (numBytes >= 6) {
        Time correctTime{};

        correctTime.second = Wire.read();
        correctTime.minute = Wire.read();
        correctTime.hour = Wire.read();
        correctTime.day = Wire.read();
        correctTime.month = Wire.read();
        correctTime.year = Wire.read();

        Serial.println("==========Received==========");
        Serial.print(correctTime.hour);
        Serial.print(":");
        Serial.print(correctTime.minute);
        Serial.print(":");
        Serial.print(correctTime.second);
        Serial.println();
        Serial.println("============================");


        calibrateRTCWithTime(&rtc, &correctTime);
    }
}

char txTime[6]; // To store the time

void i2cSendTime() {
    Time time{};

    getRealTime(&rtc, &time);

    txTime[0] = time.second;
    txTime[1] = time.minute;
    txTime[2] = time.hour;
    txTime[3] = time.day;
    txTime[4] = time.month;
    txTime[5] = time.year - 2000;
    Wire.write(txTime);
}

void shutdownUSB() {
    Serial.end();
    USBDevice.detach();

    USB->DEVICE.CTRLA.bit.ENABLE = 0;
}

void startUpUSB() {
    USB->DEVICE.CTRLA.bit.ENABLE = 1;

    USBDevice.attach();
    Serial.begin(9600);
}

void startUpI2C() {
    Wire.begin(0x72); // Initialize I2C (Slave Mode: address=0x72)
    Wire.onReceive(i2cReceiveTime);
    Wire.onRequest(i2cSendTime);
}

void reducePower() {

    // Turn off the NeoPixel
    pinMode(12, OUTPUT);
    digitalWrite(12, LOW);

    // Go down to 8MHz, so we can turn off the DFLL
    SYSCTRL->OSC8M.bit.PRESC = 0x0;
    SYSCTRL->OSC8M.bit.ENABLE = 1;

    GCLK->GENCTRL.reg =
            GCLK_GENCTRL_ID(0) |
            GCLK_GENCTRL_SRC_OSC8M |
            GCLK_GENCTRL_GENEN;

    // Turn off USB
    shutdownUSB();

    // Fix DAC, as it used the DFLL

    // Set up a new clock generator
    // GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(3) |    // Improved duty cycle
    //                 GCLK_GENCTRL_IDC |          // Enable generic clock generator
    //                 GCLK_GENCTRL_SRC_OSC8M |    // OSC8M as clock source
    //                 GCLK_GENCTRL_GENEN;         // Select GCLK3
    // while (GCLK->STATUS.bit.SYNCBUSY);
    //
    // // Connect the counters to it
    // GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | // Enable generic clock
    //                     GCLK_CLKCTRL_GEN_GCLK3 | // Select GCLK3
    //                     GCLK_CLKCTRL_ID_DAC; // Feed GCLK3 to TCC0/TCC1
    // while (GCLK->STATUS.bit.SYNCBUSY);

    // Turn off the DFLL
    SYSCTRL->DFLLCTRL.bit.ENABLE = 0;

    // Turn off the DPLL?
    SYSCTRL->DPLLCTRLA.bit.ENABLE = 0;

    // Turn off the OSC32K clock
    SYSCTRL->OSC32K.bit.ENABLE = 0;
}

void goToSleep() {
    shutdownUSB();
    rtc.standbyMode();
}

bool shouldRemind = false;
#ifdef WBTB_TIMER
bool hasReminded = false;

bool wasTouched = false;
bool timerRunning = false;
bool timerUp = false;

Time remindTime;
Time timerTime;
#endif

Time currentTime;

void interrupt() {
#ifdef WBTB_TIMER

    getRTCAsTime(&rtc, &currentTime);
    if (!timerRunning) { // don't do this while timer is running
        // maintain normal reminder functionality
        if (remindTime.hour == currentTime.hour && remindTime.minute == currentTime.minute) {
            // it's the right time, but have we reminded already this minute?
            if (!hasReminded) {
                // we haven't yet, so we will
                shouldRemind = true;
                hasReminded = true;
            }
        } else {
            // reset
            shouldRemind = false;
            hasReminded = false;
        }
    }

    if (timerRunning) {
        // time is up
        if (timerTime.hour == currentTime.hour && timerTime.minute == currentTime.minute) {
            timerRunning = false;
            timerUp = true;
        }
    }

    if (qt.measure() > TOUCH_THRESH_HOLD) {
        if (timerUp) {
            timerUp = false; // acknowledge alarm
            wasTouched = true;
        } else {
            addMinutes(&currentTime, 65 * 4);

            timerTime.hour = currentTime.hour;
            timerTime.minute = currentTime.minute;
            timerRunning = true;

            wasTouched = true;
        }
    }

    rtc.disableAlarm();
    rtc.detachInterrupt();

#else
    shouldRemind = true;
#endif
}

void setup() {
    Serial.begin(9600);
    reducePower();

    startUpI2C();
    pinMode(0, OUTPUT);

    DELAY(2000);

    rtc.begin(); // initialize RTC

#ifdef WBTB_TIMER
    qt.begin();
#endif

    currentTime.year = 2025;
    currentTime.month = 7;
    currentTime.day = 1;
    currentTime.hour = 18;
    currentTime.minute = 30;
    currentTime.second = 0;

    calibrateRTCWithTime(&rtc, &currentTime);

    shouldRemind = true; // for testing purposes
}

void loop() {
#ifdef TESTING_THRESH_HOLD
    Serial.println(qt.measure());

    if (qt.measure() > TOUCH_THRESH_HOLD) {
        digitalWrite(0, 1);
    } else {
        digitalWrite(0, 0);
    }

    return;
#endif

    getRTCAsTime(&rtc, &currentTime);

#ifdef WBTB_TIMER
    addSeconds(&currentTime, POLL_TIME);
#else
    addMinutes(&currentTime, lround(random(5, 30) * CORRECTION_FACTOR));
#endif

    rtc.setAlarmTime(currentTime.hour, currentTime.minute, currentTime.second);
    // rtc.setAlarmDate(currentTime.day, currentTime.month, currentTime.year);
    rtc.enableAlarm(RTCZero::MATCH_HHMMSS);
    rtc.attachInterrupt(interrupt);

#ifdef WBTB_TIMER
    getRTCAsTime(&rtc, &currentTime);
    addMinutes(&currentTime, lround(random(5, 30) * CORRECTION_FACTOR));
    remindTime.hour = currentTime.hour;
    remindTime.minute = currentTime.minute;
#endif

    if (!shouldRemind
#ifdef WBTB_TIMER
        || !wasTouched
        || !timerUp
#endif
        ) {
        goToSleep();
    }

#ifdef WBTB_TIMER
    if (wasTouched) {
        digitalWrite(0, 1);
        DELAY(70);
        digitalWrite(0, 0);
        DELAY(180);
        digitalWrite(0, 1);
        DELAY(70);
        digitalWrite(0, 0);

        wasTouched = false;
    }

    if (timerUp) {
        digitalWrite(0, 1);
        DELAY(1000);
        digitalWrite(0, 0);
        DELAY(500);
        digitalWrite(0, 1);
        DELAY(1000);
        digitalWrite(0, 0);
        DELAY(500);
        digitalWrite(0, 1);
        DELAY(1000);
        digitalWrite(0, 0);
        DELAY(500);
    }
#endif

    if (shouldRemind) {
        digitalWrite(0, 1);
        DELAY(70);
        digitalWrite(0, 0);
        DELAY(180);
        digitalWrite(0, 1);
        DELAY(70);
        digitalWrite(0, 0);
        DELAY(180);
        digitalWrite(0, 1);
        DELAY(70);
        digitalWrite(0, 0);

        shouldRemind = false;
#ifndef WBTB_TIMER
        rtc.disableAlarm();
        rtc.detachInterrupt();
#endif
    }
}