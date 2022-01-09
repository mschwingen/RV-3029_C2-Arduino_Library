/*
  Setting and reading time from RV-3029-C2 Real Time Clock
  By: Constantin Koch, Michael Schwingen
  Date: 2022-01-09
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  https://github.com/mschwingen/RV-3029_C2-Arduino_Library

  This example shows how to set the time on the RTC to the compiler time or a custom time, and how to read the time.
  Open the serial monitor at 115200 baud.
*/

#include <Arduino.h>
#include <Wire.h>

#include <RV-3029-C2.h>

RV3029 rtc;

//The below variables control what the date will be set to
int sec = 33;
int minute = 59;
int hour = 23;
int day = 31;
int month = 12;
int year = 2021;

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
}

void loop() {
  static int clkout = 0;
  static int trickle = 0;
  //PRINT TIME
  if (!rtc.updateTime()) //Updates the time variables from RTC
  {
    Serial.print("RTC failed to update");
    delay(1000);
  } else {
    Serial.println(rtc.stringTimeStamp());
#if 0
    Serial.print(rtc.stringTime());
    Serial.print("|");
    Serial.print(rtc.stringDate());
    Serial.print("|");
    Serial.print(rtc.stringDateUSA());
    Serial.print(", weekday=");
    Serial.println(rtc.getWeekday());
#endif
    Serial.println("\'s\' = set time, \'d\' = dump registers, \'O\' = toggle CLKOUT, \'T\' = toggle trickle charging, \'t\' = timer test");
    if (rtc.readTimerInterruptFlag()) {
      Serial.println("Timer INT!\n");
      rtc.clearTimerInterruptFlag();
    }
    Serial.printf("Temperature: %d°C\n", rtc.getTemperature());
    delay(500);
  }
  //SET TIME?
  if (Serial.available()) {
    switch (Serial.read()) {
      case 's':
#if 1
	// set the RTC to your own time
        if (rtc.setTime(sec, minute, hour, day, month, year) == false) {
          Serial.println("Something went wrong setting the time");
	}
#else
        //Use the time from the Arduino compiler (build time) to set the RTC
        if (rtc.setToCompilerTime() == false) {
          Serial.println("Something went wrong setting the time");
        }
#endif
        break;
      case 'd':
	rtc.dumpRegisters();
	break;
      case 'O': // switch to next CLKOUT option
	clkout++;
	switch(clkout)
	{
	  default:
	    clkout = 0;
	  case 0:
	    rtc.disableClockOut();
	    Serial.println("CLKOUT disabled.");
	    break;
	  case 1:
	    rtc.enableClockOut(RV3029::ClkOutFreq::CLK_1HZ);
	    Serial.println("CLKOUT 1Hz");
	    break;
	  case 2:
	    rtc.enableClockOut(RV3029::ClkOutFreq::CLK_32HZ);
	    Serial.println("CLKOUT 32Hz");
	    break;
	  case 3:
	    rtc.enableClockOut(RV3029::ClkOutFreq::CLK_1024HZ);
	    Serial.println("CLKOUT 1024Hz");
	    break;
	  case 4:
	    rtc.enableClockOut(RV3029::ClkOutFreq::CLK_32768HZ);
	    Serial.println("CLKOUT 32768Hz");
	    break;
	}
	break;
      case 'T': // switch to next trickle charge option
	trickle++;
	switch(trickle)
	{
	  default:
	    trickle = 0;
	  case 0:
	    rtc.disableTrickleCharge();
	    Serial.println("Trickle charge off");
	    break;
	  case 1:
	    rtc.enableTrickleCharge(RV3029::TrickleCharge::TCR_80K);
	    Serial.println("Trickle charge 80k");
	    break;
	  case 2:
	    rtc.enableTrickleCharge(RV3029::TrickleCharge::TCR_20K);
	    Serial.println("Trickle charge 20k");
	    break;
	  case 3:
	    rtc.enableTrickleCharge(RV3029::TrickleCharge::TCR_5K);
	    Serial.println("Trickle charge 5k");
	    break;
	  case 4:
	    rtc.enableTrickleCharge(RV3029::TrickleCharge::TCR_1K);
	    Serial.println("Trickle charge 1k");
	    break;
	}
	break;
      case 't': // activate periodic timer interrupt
	rtc.setTimer(true, RV3029::TimerFreq::TF_8HZ, 5*8, 1, 1); // set for 5s
	break;
    }
  }
}
