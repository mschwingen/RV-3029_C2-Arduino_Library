/*
  Setting alarm interrupts at RV-3029-C2 Real Time Clock
  By: Constantin Koch, Michael Schwingen
  Date: 2022-01-09
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  https://github.com/mschwingen/RV-3029_C2-Arduino_Library

  This example shows how to set alarm interrupts at RV-3029-C2 Real Time Clock.
  Open the serial monitor at 115200 baud
*/

#include <Arduino.h>
#include <Wire.h>
#include <RV-3029-C2.h>

RV3029 rtc;

//The below variables control what the date will be set to
int sec = 45;
int minute = 50;
int hour = 19;
int day = 5;
int month = 8;
int year = 2019;

//The below variables control what the alarm will be set to
int alm_sec = 50;
int alm_minute = 50;
int alm_hour = 19;
int alm_day = 5;
int alm_month = 8;
int alm_year = 2019;

# define SCL_PIN 4
# define SDA_PIN 16

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Read/Write Time - RTC Example");

  Wire.begin(SDA_PIN, SCL_PIN);
  if (rtc.begin() == false) {
    Serial.println("Something went wrong, check wiring");
    while (1);
  }
  else
    Serial.println("RTC online!");
  delay(1000);

  rtc.setAlarm(alm_sec, alm_minute, alm_hour,
	       alm_day, alm_month, alm_year,
	       AlarmFlags::AL_SEC | AlarmFlags::AL_MIN | AlarmFlags::AL_HOUR | AlarmFlags::AL_DAY | AlarmFlags::AL_WDAY | AlarmFlags::AL_MON | AlarmFlags::AL_YEAR);
  rtc.enableAlarmInterrupt();
  //rtc.disableAlarmInterrupt();  //Only disables the interrupt (not the alarm flag)
}

void loop() {
  //PRINT TIME
  if (!rtc.updateTime()) //Updates the time variables from RTC
  {
    Serial.print("RTC failed to update");
  } else {
    Serial.print(rtc.stringTimeStamp());
    Serial.println("     \'s\' = set time, \'d\' = dump registers, ");
    delay(250);
  }

  //Read Alarm Flag
  if (rtc.readAlarmInterruptFlag()) {
    Serial.println("ALARM!!!!");
    rtc.clearAlarmInterruptFlag();
    delay(500);
  }

  //SET TIME???
  if (Serial.available()) {
    switch (Serial.read()) {
      case 's':
#if 0
        //Use the build time
        if (!rtc.setToCompilerTime()) {
          Serial.println("Something went wrong setting the time");
	}
#else
        if (!rtc.setTime(sec, minute, hour, day, month, year)) {
          Serial.println("Something went wrong setting the time");
        }
#endif
        break;
      case 'd':
	rtc.dumpRegisters();
	break;
    }
  }
}

