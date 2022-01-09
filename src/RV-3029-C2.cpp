/******************************************************************************
RV-3029-C2.h
RV-3029-C2 Arduino Library
Michael Schwingen, based on RV-3028-C2 library by Constantin Koch
2022-01-09
https://github.com/mschwingen/RV-3029_C2-Arduino_Library

This code is released under the [MIT License](http://opensource.org/licenses/MIT).
Please review the LICENSE.md file included with this example. If you have any questions
or concerns with licensing, please contact constantinkoch@outlook.com.
Distributed as-is; no warranty is given.
******************************************************************************/

#include "RV-3029-C2.h"
#include "build_date.h"

//The 7-bit I2C ADDRESS of the RV3029
#define RV3029_ADDR				((uint8_t) 0x56)

// register addresses
enum class RV3029::RegAddr: uint8_t {
  CONTROL_1 = 0x00,
  CONTROL_INT = 0x01,
  CONTROL_INT_FLAG = 0x02,
  CONTROL_STATUS = 0x03,
  CONTROL_RESET = 0x04,

  CLK_SECONDS = 0x08,
  CLK_MINUTES = 0x09,
  CLK_HOURS = 0x0A,
  CLK_DAYS = 0x0B,
  CLK_WEEKDAYS = 0x0C,
  CLK_MONTHS = 0x0D,
  CLK_YEARS = 0x0E,

  ALM_SECONDS = 0x10,
  ALM_MINUTES = 0x11,
  ALM_HOURS = 0x12,
  ALM_DAYS = 0x13,
  ALM_WEEKDAYS = 0x14,
  ALM_MONTHS = 0x15,
  ALM_YEARS = 0x16,

  TIMER_LOW = 0x18,
  TIMER_HIGH = 0x19,

  TEMPERATURE = 0x20,

  EEPROM0 = 0x28,
  EEPROM1 = 0x29,

  EEPROM_CTRL = 0x30,
  XTAL_OFFSET = 0x31,
  XTAL_COEFF = 0x32,
  XTAL_T0 = 0x33,

  RAM0 = 0x38,
  RAM1 = 0x39,
  RAM2 = 0x3a,
  RAM3 = 0x3b,
  RAM4 = 0x3c,
  RAM5 = 0x3d,
  RAM6 = 0x3e,
  RAM7 = 0x3f
};

// bit masks for control registers
#define CONTROL_1_WE		(0x01)
#define CONTROL_1_TE 		(0x02)
#define CONTROL_1_TAR		(0x04)
#define CONTROL_1_EERE		(0x08)
#define CONTROL_1_SROn		(0x10)
#define CONTROL_1_TD0		(0x20)
#define CONTROL_1_TD1		(0x40)
#define CONTROL_1_CLK_INT	(0x80)

#define CONTROL_INT_AIE		(0x01)
#define CONTROL_INT_TIE		(0x02)
#define CONTROL_INT_V1IE	(0x04)
#define CONTROL_INT_V2IE	(0x08)
#define CONTROL_INT_SRIE	(0x10)

#define CONTROL_INT_FLAG_AF	(0x01)
#define CONTROL_INT_FLAG_TF	(0x02)
#define CONTROL_INT_FLAG_V1IF	(0x04)
#define CONTROL_INT_FLAG_V2IF	(0x08)
#define CONTROL_INT_FLAG_SRF	(0x10)

#define CONTROL_STATUS_V1F	(0x04)
#define CONTROL_STATUS_V2F	(0x08)
#define CONTROL_STATUS_SR	(0x10)
#define CONTROL_STATUS_PON	(0x20)
#define CONTROL_STATUS_EEBUSY	(0x80)

#define CONTROL_RESET_SYSR	(0x10)

#define EEPROM_CTRL_THP		(0x01)
#define EEPROM_CTRL_THE		(0x02)
#define EEPROM_CTRL_FD0		(0x04)
#define EEPROM_CTRL_FD1		(0x08)
#define EEPROM_CTRL_R1K		(0x10)
#define EEPROM_CTRL_R5K		(0x20)
#define EEPROM_CTRL_R20K	(0x40)
#define EEPROM_CTRL_R80K	(0x80)

#define TIME_ARRAY_LENGTH 7 // Total number of writable values in device
enum time_order {
  TIME_SECONDS = 0,
  TIME_MINUTES = 1,
  TIME_HOURS   = 2,
  TIME_DAY     = 3,
  TIME_WEEKDAY = 4,
  TIME_MONTH   = 5,
  TIME_YEAR    = 6,
};

bool RV3029::begin(TwoWire &wirePort) {
  bool rc;
  uint8_t cs;
  _i2cPort = &wirePort;

  // disable all interrupts
  rc = writeRegister(RegAddr::CONTROL_INT, 0);
  rc &= writeRegister(RegAddr::CONTROL_INT_FLAG, 0);
  // Read PON bit, if set: execute System reset, clear PON and SR Flags
  rc &= readRegister(RegAddr::CONTROL_STATUS, cs);
  if (!rc)
    return false;

  if (cs & (CONTROL_STATUS_PON | CONTROL_STATUS_SR | CONTROL_STATUS_V2F)) {
    time_valid = false;
    cs &= ~(CONTROL_STATUS_PON | CONTROL_STATUS_SR | CONTROL_STATUS_V1F | CONTROL_STATUS_V2F);
    rc = writeRegister(RegAddr::CONTROL_RESET, CONTROL_RESET_SYSR);
    delay(1);
    rc &= writeRegister(RegAddr::CONTROL_STATUS, cs);
    return rc;
  }

  return updateTime();
}

void RV3029::dumpRegisters(void) {
  for (unsigned reg = 0; reg <= 0x3F; reg++) {
    bool rc;
    uint8_t data;
    if ((reg & 7) == 0)
      printf("%02x: ", reg);
    rc = readRegister((RegAddr) reg, data);
    if (rc)
      printf("%02X ", data);
    else
      printf("-- ");
    if ((reg & 7) == 7)
      printf("\n");
  }
}

bool RV3029::setTime(struct tm *newtime) {
  cur_time = *newtime;
  return writeTime(cur_time, RegAddr::CLK_SECONDS, AL_NONE);
}

bool RV3029::setTime(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t month, uint16_t year) {
  cur_time.tm_sec = sec;
  cur_time.tm_min = min;
  cur_time.tm_hour = hour;
  cur_time.tm_mday = day;
  cur_time.tm_mon = month - 1;
  cur_time.tm_year = year - 1900;
  return writeTime(cur_time, RegAddr::CLK_SECONDS, AL_NONE);
}

// write time to RTC registers, either clock or alarm registers, starting at
// address "dest".
bool RV3029::writeTime(struct tm &tm, RegAddr dest, AlarmFlags alarm) {
  uint8_t timeregs[TIME_ARRAY_LENGTH];

  // Calculate weekday (from https://web.archive.org/web/20060615232017/http://users.aol.com/s6sj7gt/mikecal.htm)
  // 0 = Sunday, 6 = Saturday
  uint16_t d = tm.tm_mday;
  uint16_t m = tm.tm_mon + 1;
  uint16_t y = tm.tm_year + 1900;
  tm.tm_wday = (d += m < 3 ? y-- : y - 2, 23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400) % 7;

  timeregs[TIME_SECONDS] = DECtoBCD(tm.tm_sec);
  timeregs[TIME_MINUTES] = DECtoBCD(tm.tm_min);
  timeregs[TIME_HOURS] = DECtoBCD(tm.tm_hour);
  timeregs[TIME_DAY] = DECtoBCD(tm.tm_mday);
  timeregs[TIME_MONTH] = DECtoBCD(tm.tm_mon) + 1;
  timeregs[TIME_YEAR] = DECtoBCD(tm.tm_year - 100);
  timeregs[TIME_WEEKDAY] = DECtoBCD(tm.tm_wday + 1);

  if (alarm & AL_SEC)
    timeregs[TIME_SECONDS] |= 0x80;
  if (alarm & AL_MIN)
    timeregs[TIME_MINUTES] |= 0x80;
  if (alarm & AL_HOUR)
    timeregs[TIME_HOURS] |= 0x80;
  if (alarm & AL_DAY)
    timeregs[TIME_DAY] |= 0x80;
  if (alarm & AL_MON)
    timeregs[TIME_MONTH] |= 0x80;
  if (alarm & AL_YEAR)
    timeregs[TIME_YEAR] |= 0x80;
  if (alarm & AL_WDAY)
    timeregs[TIME_WEEKDAY] |= 0x80;

  bool rc = writeMultipleRegisters(dest, timeregs, TIME_ARRAY_LENGTH);
  if (rc)
    time_valid = true;
  return rc;
}

//Takes the build time and uses it as the current time
bool RV3029::setToCompilerTime() {
  return setTime(BUILD_SECOND, BUILD_MINUTE, BUILD_HOUR, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
}

//Read the time registers from RV-3029-C2
//Needs to be called before printing time or date
bool RV3029::updateTime(struct tm *cur) {
  uint8_t timeregs[TIME_ARRAY_LENGTH];

  if (readMultipleRegisters(RegAddr::CLK_SECONDS, timeregs, TIME_ARRAY_LENGTH) == false)
    return(false);

  timeregs[TIME_HOURS] &= 0x3F;

  cur_time.tm_sec = BCDtoDEC(timeregs[TIME_SECONDS]);
  cur_time.tm_min = BCDtoDEC(timeregs[TIME_MINUTES]);
  cur_time.tm_hour = BCDtoDEC(timeregs[TIME_HOURS]);
  cur_time.tm_mday = BCDtoDEC(timeregs[TIME_DAY]);
  cur_time.tm_mon = BCDtoDEC(timeregs[TIME_MONTH]) - 1;
  cur_time.tm_year = BCDtoDEC(timeregs[TIME_YEAR]) + 100;
  cur_time.tm_wday = BCDtoDEC(timeregs[TIME_WEEKDAY]) - 1;
  cur_time.tm_yday = 0; // not implemented
  cur_time.tm_isdst = false;

  if (!time_valid) {
    if (cur_time.tm_year + 1900 > 2020 &&
	cur_time.tm_mon >= 0 && cur_time.tm_mon <= 11 &&
        cur_time.tm_mday >= 1 && cur_time.tm_mday <= 31 &&
	cur_time.tm_hour >= 0 && cur_time.tm_hour <= 23 &&
	cur_time.tm_min >= 0 && cur_time.tm_min <= 59 &&
	cur_time.tm_sec >= 0 && cur_time.tm_sec <= 59 )
      time_valid = true;
  }
  if (cur)
    *cur = cur_time;
  return true;
}

//Returns a pointer to array of chars that are the date in mm/dd/yyyy format
char* RV3029::stringDateUSA() {
  static char date[11]; //Max of mm/dd/yyyy with \0 terminator
  if (time_valid)
    sprintf(date, "%02u/%02u/%04u", getMonth(), getDay(), getYear());
  else
    strcpy(date, "invalid");
  return(date);
}

//Returns a pointer to array of chars that are the date in dd.mm.yyyy format
char*  RV3029::stringDate() {
  static char date[11]; //Max of dd/mm/yyyy with \0 terminator
  if (time_valid)
    sprintf(date, "%02u.%02u.%04u", getDay(), getMonth(), getYear());
  else
    strcpy(date, "invalid");
  return(date);
}

//Returns a pointer to array of chars that represents the time in hh:mm:ss format
char* RV3029::stringTime() {
  static char time[11]; //Max of hh:mm:ssXM with \0 terminator
  if (time_valid)
    sprintf(time, "%02u:%02u:%02u", getHours(), getMinutes(), getSeconds());
  else
    strcpy(time, "invalid");
  return(time);
}

char* RV3029::stringTimeStamp() {
  static char timeStamp[25]; //Max of yyyy-mm-dd hh:mm:ss.ss with \0 terminator

  if (time_valid)
    sprintf(timeStamp, "%04u-%02u-%02u %02u:%02u:%02u",
	    getYear(), getMonth(), getDay(),
	    getHours(), getMinutes(), getSeconds());
  else
    strcpy(timeStamp, "invalid");
  return(timeStamp);
}

uint8_t RV3029::getSeconds() {
  return cur_time.tm_sec;
}

uint8_t RV3029::getMinutes() {
  return cur_time.tm_min;
}

uint8_t RV3029::getHours() {
  return cur_time.tm_hour;
}

uint8_t RV3029::getWeekday() {
  return cur_time.tm_wday;
}

uint8_t RV3029::getDay() {
  return cur_time.tm_mday;
}

uint8_t RV3029::getMonth() {
  return cur_time.tm_mon + 1;
}

uint16_t RV3029::getYear() {
  return cur_time.tm_year + 1900;
}

// Set alarm. "which" is a bitmask which specifies which fields need to
// match to trigger an alarm interrupt.
bool RV3029::setAlarm(struct tm &newtime, AlarmFlags which) {
  return writeTime(newtime, RegAddr::ALM_SECONDS, which);
}

bool RV3029::setAlarm(uint8_t sec, uint8_t min, uint8_t hour,
		      uint8_t day, uint8_t month, uint16_t year,
		      AlarmFlags which) {
  struct tm tm;
  tm.tm_sec = sec;
  tm.tm_min = min;
  tm.tm_hour = hour;
  tm.tm_mday = day;
  tm.tm_mon = month - 1;
  tm.tm_year = year - 1900;
  return writeTime(tm, RegAddr::ALM_SECONDS, which);
}

void RV3029::enableAlarmInterrupt() {
  setBit(RegAddr::CONTROL_INT, CONTROL_INT_AIE);
}

void RV3029::disableAlarmInterrupt() {
  clearBit(RegAddr::CONTROL_INT, CONTROL_INT_AIE);
}

bool RV3029::readAlarmInterruptFlag() {
  return readBit(RegAddr::CONTROL_INT_FLAG, CONTROL_INT_FLAG_AF);
}

void RV3029::clearAlarmInterruptFlag() {
  clearBit(RegAddr::CONTROL_INT_FLAG, CONTROL_INT_FLAG_AF);
}

// Countdown Timer
bool RV3029::setTimer(bool timer_repeat, TimerFreq timer_frequency,
		      uint16_t timer_value, bool set_interrupt, bool start_timer)
{
  bool rc;

  disableTimer();
  disableTimerInterrupt();
  clearTimerInterruptFlag();

  rc = writeRegister(RegAddr::TIMER_LOW, timer_value & 0xff);
  rc &= writeRegister(RegAddr::TIMER_HIGH, timer_value >> 8);

  uint8_t ctrl1_val;
  rc &= readRegister(RegAddr::CONTROL_1, ctrl1_val);
  if (timer_repeat)
    ctrl1_val |= CONTROL_1_TAR;
  else
    ctrl1_val &= ~CONTROL_1_TAR;

  switch (timer_frequency)
  {
    case TimerFreq::TF_0_5HZ:
      ctrl1_val |=  CONTROL_1_TD0;
      ctrl1_val |=  CONTROL_1_TD1;
      break;

    case TimerFreq::TF_1HZ:
      ctrl1_val &= ~CONTROL_1_TD0;
      ctrl1_val |=  CONTROL_1_TD1;
      break;

    case TimerFreq::TF_8HZ:
      ctrl1_val |=  CONTROL_1_TD0;
      ctrl1_val &= ~CONTROL_1_TD1;
      break;

    case TimerFreq::TF_32HZ:
      ctrl1_val &= ~CONTROL_1_TD0;
      ctrl1_val &= ~CONTROL_1_TD1;
      break;
  }

  if (start_timer)
    ctrl1_val |= CONTROL_1_TE;
  rc &= writeRegister(RegAddr::CONTROL_1, ctrl1_val);
  if (set_interrupt)
    enableTimerInterrupt();
  return rc;
}

void RV3029::enableTimerInterrupt() {
  setBit(RegAddr::CONTROL_INT, CONTROL_INT_TIE);
}

void RV3029::disableTimerInterrupt() {
  clearBit(RegAddr::CONTROL_INT, CONTROL_INT_TIE);
}

bool RV3029::readTimerInterruptFlag() {
  return readBit(RegAddr::CONTROL_INT_FLAG, CONTROL_INT_FLAG_TF);
}

void RV3029::clearTimerInterruptFlag() {
  clearBit(RegAddr::CONTROL_INT_FLAG, CONTROL_INT_FLAG_TF);
}

void RV3029::enableTimer() {
  setBit(RegAddr::CONTROL_1, CONTROL_1_TE);
}

void RV3029::disableTimer() {
  clearBit(RegAddr::CONTROL_1, CONTROL_1_TE);
}

int RV3029::getTemperature() {
  bool rc;
  uint8_t temp8;
  rc = readRegister(RegAddr::TEMPERATURE, temp8);
  if (rc)
    return (int) temp8 - 60;
  return -999;
}


// Enable the Trickle Charger and set the Trickle Charge series resistor
bool RV3029::enableTrickleCharge(TrickleCharge tcr) {
  bool rc;
  uint8_t ctrl = 0;
  switch(tcr) {
    case TrickleCharge::TCR_1K:
      ctrl = EEPROM_CTRL_R1K;
      break;
    case TrickleCharge::TCR_5K:
      ctrl = EEPROM_CTRL_R5K;
      break;
    case TrickleCharge::TCR_20K:
      ctrl = EEPROM_CTRL_R20K;
      break;
    case TrickleCharge::TCR_80K:
      ctrl= EEPROM_CTRL_R80K;
      break;
  }
  rc = eeprom_control(EEPROM_CTRL_R1K | EEPROM_CTRL_R5K | EEPROM_CTRL_R20K | EEPROM_CTRL_R80K, ctrl);
  return rc;
}

bool RV3029::disableTrickleCharge() {
  return eeprom_control(EEPROM_CTRL_R1K | EEPROM_CTRL_R5K | EEPROM_CTRL_R20K | EEPROM_CTRL_R80K, 0);
}

// set CLKOUT pin
bool RV3029::enableClockOut(ClkOutFreq freq) {
  setBit(RegAddr::CONTROL_1, CONTROL_1_CLK_INT);
  uint8_t ctrl;
  switch(freq)
  {
    case RV3029::ClkOutFreq::CLK_1HZ:
      ctrl = EEPROM_CTRL_FD0 | EEPROM_CTRL_FD1;
      break;
    case RV3029::ClkOutFreq::CLK_32HZ:
      ctrl = EEPROM_CTRL_FD1;
      break;
    case RV3029::ClkOutFreq::CLK_1024HZ:
      ctrl = EEPROM_CTRL_FD0;
      break;
    case RV3029::ClkOutFreq::CLK_32768HZ:
    default:
      ctrl = 0;
      break;
  }
  bool rc = eeprom_control(EEPROM_CTRL_FD0 | EEPROM_CTRL_FD1, ctrl);
  return rc;
}

void RV3029::disableClockOut() {
  clearBit(RegAddr::CONTROL_1, CONTROL_1_CLK_INT);
}

// update EEPROM_CONTROL register
bool RV3029::eeprom_control(uint8_t clear_bits, uint8_t set_bits) {
  uint8_t ctrl, ctrl2;
  bool rc;
  rc = readRegister(RegAddr::EEPROM_CTRL, ctrl);
  if (!rc)
    return false;
  ctrl2 = ctrl & ~clear_bits;
  ctrl2 |= set_bits;
  if (ctrl2 == ctrl) // no change needed
    return true;
  rc = eeprom_write(RegAddr::EEPROM_CTRL, ctrl2);
  return rc;
}

uint8_t RV3029::BCDtoDEC(uint8_t val) {
  return ((val / 0x10) * 10) + (val % 0x10);
}

uint8_t RV3029::DECtoBCD(uint8_t val) {
  return ((val / 10) * 0x10) + (val % 10);
}

bool RV3029::readRegister(RegAddr addr, uint8_t &result) {
  _i2cPort->beginTransmission(RV3029_ADDR);
  _i2cPort->write((uint8_t) addr);
  _i2cPort->endTransmission();

  _i2cPort->requestFrom(RV3029_ADDR, (uint8_t) 1);
  if (_i2cPort->available()) {
    result = _i2cPort->read();
    return true;
  }
  else {
    result = 0x00;
    return false; //Error
  }
}

bool RV3029::writeRegister(RegAddr addr, uint8_t val) {
  _i2cPort->beginTransmission(RV3029_ADDR);
  _i2cPort->write((uint8_t) addr);
  _i2cPort->write(val);
  return _i2cPort->endTransmission() == 0;
}

bool RV3029::readMultipleRegisters(RegAddr addr, uint8_t * dest, uint8_t len) {
  _i2cPort->beginTransmission(RV3029_ADDR);
  _i2cPort->write((uint8_t) addr);
  if (_i2cPort->endTransmission() != 0)
    return (false); //Error: Sensor did not ack

  _i2cPort->requestFrom(RV3029_ADDR, len);
  for (uint8_t i = 0; i < len; i++)
  {
    dest[i] = _i2cPort->read();
  }

  return(true);
}

bool RV3029::writeMultipleRegisters(RegAddr addr, uint8_t * values, uint8_t len) {
  _i2cPort->beginTransmission(RV3029_ADDR);
  _i2cPort->write((uint8_t) addr);
  for (uint8_t i = 0; i < len; i++)
  {
    _i2cPort->write(values[i]);
  }

  if (_i2cPort->endTransmission() != 0)
    return (false); //Error: Sensor did not ack
  return(true);
}

bool RV3029::setBit(RegAddr reg_addr, uint8_t mask) {
  bool rc;
  uint8_t value;
  rc = readRegister(reg_addr, value);
  value |= mask;
  rc &= writeRegister(reg_addr, value);
  return rc;
}

bool RV3029::clearBit(RegAddr reg_addr, uint8_t mask) {
  bool rc;
  uint8_t value;
  rc = readRegister(reg_addr, value);
  value &= ~mask;
  rc &= writeRegister(reg_addr, value);
  return rc;
}

bool RV3029::readBit(RegAddr reg_addr, uint8_t mask) {
  uint8_t value;
  readRegister(reg_addr, value);
  value &= mask;
  return !!value;
}

bool RV3029::eeprom_write(RegAddr addr, uint8_t val) {
  bool rc;
  rc = eeprom_enter();
  if (!rc)
    return false;
  rc = writeRegister(addr, val);
  rc &= eeprom_busywait();
  rc &= eeprom_exit();
  return rc;
}

bool RV3029::eeprom_enter(void) {
  bool rc;
  uint8_t cs;
  rc = readRegister(RegAddr::CONTROL_STATUS, cs);
  if (!rc)
    return false;

  // check if voltage is high enough for EEPROM operation
  if (cs & ( CONTROL_STATUS_V1F | CONTROL_STATUS_V2F)) {
    cs &= ~(CONTROL_STATUS_V1F | CONTROL_STATUS_V2F);
    rc = writeRegister(RegAddr::CONTROL_STATUS, cs);
    // might have been a prior brownout, retry once
    rc &= readRegister(RegAddr::CONTROL_STATUS, cs);
    if (!rc)
      return false;
  }
  if (cs & ( CONTROL_STATUS_V1F | CONTROL_STATUS_V2F)) {
    return false;
  }
  if (!clearBit(RegAddr::CONTROL_1, CONTROL_1_EERE))
    return false;
  return eeprom_busywait();
}

bool RV3029::eeprom_exit(void) {
  return setBit(RegAddr::CONTROL_1, CONTROL_1_EERE);
}

bool RV3029::eeprom_busywait(void) {
  uint8_t cs;
  for (int i=0; i<100; i++) {
    if (!readRegister(RegAddr::CONTROL_STATUS, cs))
      return false;
    if ((cs & CONTROL_STATUS_EEBUSY) == 0)
      return true;
    delay(1);
  }
  return false;
}
