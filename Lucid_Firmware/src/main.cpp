#include <Arduino.h>
#include <RTCZero.h>
#include <RealTime.h>
#include <Wire.h>

#define DELAY(value) delay(value / 6)

RTCZero rtc;

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

void remind() {
    shouldRemind = true;
}

Time currentTime;

void setup() {
    Serial.begin(9600);
    reducePower();

    startUpI2C();
    pinMode(0, OUTPUT);

    DELAY(2000);

    rtc.begin(); // initialize RTC

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

    getRTCAsTime(&rtc, &currentTime);

    addMinutes(&currentTime, lround(random(5, 30)  * CORRECTION_FACTOR));
    // addSeconds(&currentTime, 4);

    rtc.setAlarmTime(currentTime.hour, currentTime.minute, currentTime.second);
    // rtc.setAlarmDate(currentTime.day, currentTime.month, currentTime.year);
    rtc.enableAlarm(RTCZero::MATCH_HHMMSS);
    rtc.attachInterrupt(remind);

    if (!shouldRemind) {
        goToSleep();
    }

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

        rtc.disableAlarm();
        rtc.detachInterrupt();
    }
}