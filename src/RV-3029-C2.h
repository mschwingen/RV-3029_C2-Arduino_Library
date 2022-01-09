/******************************************************************************
RV-3029-C2.h
RV-3029-C2 Arduino Library
Michael Schwingen
2022-01-09
https://github.com/mschwingen/RV-3029_C2-Arduino_Library

based on
RV-3028-C7 Arduino Library
Constantin Koch
July 25, 2019
https://github.com/constiko/RV-3028_C7-Arduino_Library

Resources:
Uses Wire.h for I2C operation

This code is released under the [MIT License](http://opensource.org/licenses/MIT).
Please review the LICENSE.md file included with this example. If you have any questions
or concerns with licensing, please contact michael@schwingen.org.
Distributed as-is; no warranty is given.
******************************************************************************/

#pragma once

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Wire.h>
#include <time.h>

enum AlarmFlags {
  AL_NONE = 0x00,
  AL_SEC = 0x01,  // seconds
  AL_MIN = 0x02,  // minutes
  AL_HOUR = 0x04, // hours
  AL_DAY = 0x08,  // day of month
  AL_WDAY = 0x10, // weekday
  AL_MON = 0x20,  // month
  AL_YEAR = 0x40  // year
};
inline AlarmFlags operator|(AlarmFlags a, AlarmFlags b) {
  return static_cast<AlarmFlags>(static_cast<int>(a) | static_cast<int>(b));
}
inline AlarmFlags operator&(AlarmFlags a, AlarmFlags b) {
  return static_cast<AlarmFlags>(static_cast<int>(a) & static_cast<int>(b));
}

class RV3029
{
public:
  RV3029(void) { time_valid = false; }
  bool begin(TwoWire &wirePort = Wire);

  // check if RTC time is valid
  bool isValid() { return time_valid; }

  // get/set time
  bool setTime(struct tm *newtime);
  bool updateTime(struct tm *cur = 0); // read RTC time

  // get/set date/time in human-readable notation (4-digit year, month starting at "1")
  uint8_t getSeconds();
  uint8_t getMinutes();
  uint8_t getHours();
  uint8_t getWeekday();
  uint8_t getDay();
  uint8_t getMonth();
  uint16_t getYear();
  bool setTime(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t month, uint16_t year);

  bool setToCompilerTime(); // Uses the hours, mins, etc from compile time to set RTC

  // return current time/date as string
  char* stringDateUSA();   // Return date in mm-dd-yyyy
  char* stringDate();      // Return date in dd.mm.yyyy
  char* stringTime();      // Return time hh:mm:ss
  char* stringTimeStamp(); // Return timeStamp in ISO 8601 format yyyy-mm-dd hh:mm:ss

  // Alarm functions. AlarmFlags is a bitmask that declares which fields
  // shall be compared when causing an alarm interrupt
  bool setAlarm(struct tm &newtime, AlarmFlags which);
  bool setAlarm(uint8_t sec, uint8_t min, uint8_t hour,
		uint8_t day, uint8_t month, uint16_t year,
		AlarmFlags which);
  void enableAlarmInterrupt();
  void disableAlarmInterrupt();
  bool readAlarmInterruptFlag();
  void clearAlarmInterruptFlag();

  // 16-Bit count-down timer functions
  enum class TimerFreq {
    TF_0_5HZ,
    TF_1HZ,
    TF_8HZ,
    TF_32HZ
  };
  bool setTimer(bool timer_repeat, TimerFreq timer_frequency, uint16_t timer_value, bool setInterrupt, bool start_timer);
  void enableTimer();
  void disableTimer();
  void enableTimerInterrupt();
  void disableTimerInterrupt();
  bool readTimerInterruptFlag();
  void clearTimerInterruptFlag();

  // misc.
  enum class TrickleCharge {
    TCR_80K,
    TCR_20K,
    TCR_5K,
    TCR_1K
  };
  bool enableTrickleCharge(TrickleCharge tcr = TrickleCharge::TCR_80K);
  bool disableTrickleCharge();
  enum class ClkOutFreq {
    CLK_1HZ,
    CLK_32HZ,
    CLK_1024HZ,
    CLK_32768HZ
  };
  bool enableClockOut(ClkOutFreq freq);
  void disableClockOut();
  int getTemperature();
  void dumpRegisters();

private:
  enum class RegAddr: uint8_t; // device register addresses
  bool time_valid;
  struct tm cur_time;
  TwoWire *_i2cPort;

  uint8_t BCDtoDEC(uint8_t val);
  uint8_t DECtoBCD(uint8_t val);

  bool readRegister(RegAddr addr, uint8_t &result);
  bool writeRegister(RegAddr addr, uint8_t val);
  bool readMultipleRegisters(RegAddr addr, uint8_t * dest, uint8_t len);
  bool writeMultipleRegisters(RegAddr addr, uint8_t * values, uint8_t len);
  bool setBit(RegAddr reg_addr, uint8_t mask);
  bool clearBit(RegAddr reg_addr, uint8_t mask);
  bool readBit(RegAddr reg_addr, uint8_t mask);

  bool writeTime(struct tm &tm, RegAddr dest, AlarmFlags alarm);

  bool eeprom_write(RegAddr addr, uint8_t val);
  bool eeprom_enter(void);
  bool eeprom_exit(void);
  bool eeprom_busywait(void);
  bool eeprom_control(uint8_t clear_bits, uint8_t set_bits);
};
