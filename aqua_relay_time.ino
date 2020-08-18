#define pin_relay 7
#define pin_enc_clk 2
#define pin_enc_dt 4
#define pin_enc_sw 3
#define pin_display_clk 8
#define pin_display_din 9
#define pin_display_ce 12
#define pin_display_rst 11
#define pin_display_dc 10
#define pin_display_blk 5

#include "RTClib.h"
#include <LCD5110_Basic.h>
#include "GyverEncoder.h"
#include <EEPROM.h>
#include <TimerOne.h>


int alarm_hour_on_addr = 0;
int alarm_min_on_addr = 1;
int alarm_hour_off_addr = 2;
int alarm_min_off_addr = 3;
int contrast_addr = 4;
int blk_level_addr = 5;
int blk_level_raw_addr = 6;
int relay_on_addr = 7;


volatile unsigned int year;
volatile byte month;
volatile byte day;
volatile byte hour;
volatile byte min;
volatile boolean relay_on;
volatile boolean splash;
volatile boolean relay_now;
byte alarm_hour_on;
byte alarm_min_on;
byte alarm_hour_off;
byte alarm_min_off;
byte contrast;
byte blk_level;
byte blk_level_raw;
unsigned int EEMEM addr_year;
unsigned int blk_time;
unsigned int blk_timeout;


Encoder encoder(pin_enc_clk, pin_enc_dt, pin_enc_sw);
RTC_DS1307 rtc;
LCD5110 disp(pin_display_clk, pin_display_din, pin_display_dc, pin_display_rst, pin_display_ce);

extern uint8_t SmallFont[];
extern uint8_t MediumNumbers[];

void setup() {
  pinMode(pin_relay, OUTPUT);
  pinMode(pin_display_blk, OUTPUT);
  digitalWrite(pin_relay, HIGH);
  contrast = EEPROM.read(contrast_addr);
  blk_level = EEPROM.read(blk_level_addr);
  alarm_hour_on = EEPROM.read(alarm_hour_on_addr);
  alarm_min_on = EEPROM.read(alarm_min_on_addr);
  alarm_hour_off = EEPROM.read(alarm_hour_off_addr);
  alarm_min_off = EEPROM.read(alarm_min_off_addr);
  blk_level_raw = EEPROM.read(blk_level_raw_addr);
  relay_on = EEPROM.read(relay_on_addr);
  blk_timeout = 3000;
  disp.InitLCD();
  disp.setFont(SmallFont);
  disp.setContrast(contrast);
  analogWrite(pin_display_blk, blk_level_raw);
  encoder.setType(TYPE2);
  Timer1.initialize();

  if (!rtc.begin()) {
    disp.clrScr();
    disp.print("RTC module failed", CENTER, 24);
    abort();
  }
  if (!rtc.isrunning()) {
    disp.clrScr();
    disp.print("RTC is", CENTER, 8);
    disp.print("not running", CENTER,16);
    disp.print("pls set", CENTER, 24);
    disp.print("date/time", CENTER, 32);
    while (true){
      encoder.tick();
      if (encoder.isClick() || encoder.isTurn()) break;
    }
    adjust_time();
    disp.invertText(false);
    disp.clrScr();
  }
  Timer1.attachInterrupt(disp_time);
}

void loop() {
  encoder.tick();
  if (encoder.isClick()) {
    Timer1.detachInterrupt();
    menu();
  }
  if (encoder.isHolded()){
    Timer1.detachInterrupt();
    adjust_time();
  }
  if ((millis() - blk_time) > blk_timeout) analogWrite(pin_display_blk, 255);
}

void menu() {
  byte count = 0;
  boolean ok = false;

  analogWrite(pin_display_blk, blk_level_raw);
  disp.setFont(SmallFont);
  disp.clrScr();
  disp.print("On :", LEFT, 0);
  disp.printNumI(alarm_hour_on, 42, 0, 2);
  if (alarm_hour_on < 10) disp.print("0", 42, 0);
  disp.print(".", 54, 0);
  disp.printNumI(alarm_min_on, 60, 0, 2);
  if (alarm_min_on < 10) disp.print("0", 60, 0);

  disp.print("OFF: ", LEFT, 8);
  disp.printNumI(alarm_hour_off, 42, 8, 2);
  if (alarm_hour_off < 10) disp.print("0", 42, 8);
  disp.print(".", 54, 8);
  disp.printNumI(alarm_min_off, 60, 8, 2);
  if (alarm_min_off < 10) disp.print("0", 60, 8);

  if (relay_on) disp.print("Timer ON", LEFT, 16);
  else disp.print("Timer OFF", LEFT, 16);

  disp.print("Ligth:", LEFT, 24);
  disp.printNumI(25 - blk_level, 42, 24, 3);
  disp.print("Contr:", LEFT, 32);
  disp.printNumI(contrast, 42, 32, 3);

  while (true) {
    encoder.tick();
    if (encoder.isLeft()) {
      switch (count) {
        case 1:
          if (relay_on) relay_on = false;
          else relay_on = true;
          disp.invertText(true);
          if (relay_on) disp.print("ON ", 36, 16);
          else disp.print("OFF", 36, 16);
          break;
        case 3:
          alarm_hour_on++;
          if (alarm_hour_on > 23) alarm_hour_on = 0;
          disp.invertText(true);
          disp.printNumI(alarm_hour_on, 42, 0, 2);
          if (alarm_hour_on < 10) disp.print("0", 42, 0);
          break;
        case 4:
          alarm_min_on++;
          if (alarm_min_on > 59) alarm_min_on = 0;
          disp.invertText(true);
          disp.printNumI(alarm_min_on, 60, 0, 2);
          if (alarm_min_on < 10) disp.print("0", 60, 0);
          disp.invertText(false);
          disp.printNumI(alarm_hour_on, 42, 0, 2);
          if (alarm_hour_on < 10) disp.print("0", 42, 0);
          break;
        case 5:
          alarm_hour_off++;
          if (alarm_hour_off > 23) alarm_hour_off = 0;
          disp.invertText(true);
          disp.printNumI(alarm_hour_off, 42, 8, 2);
          if (alarm_hour_off < 10) disp.print("0", 42, 8);
          disp.invertText(false);
          disp.printNumI(alarm_min_on, 60, 0, 2);
          if (alarm_min_on < 10) disp.print("0", 60, 0);
          break;
        case 6:
          alarm_min_off++;
          if (alarm_min_off > 59) alarm_min_off = 0;
          disp.invertText(true);
          disp.printNumI(alarm_min_off, 60, 8, 2);
          if (alarm_min_off < 10) disp.print("0", 60, 8);
          disp.invertText(false);
          disp.printNumI(alarm_hour_off, 42, 8, 2);
          if (alarm_hour_off < 10) disp.print("0", 42, 8);
          break;
        case 7:
          if (blk_level > 0) blk_level--;
          disp.invertText(true);
          disp.printNumI(25 - blk_level, 42, 24, 3);
          if (blk_level == 0) {
            disp.print("MAX", 42, 24);
            blk_level_raw = 0;
            analogWrite(pin_display_blk, blk_level_raw);
          } else {
            blk_level_raw = blk_level * 10;
            analogWrite(pin_display_blk, blk_level_raw);
          }
          disp.invertText(false);
          break;
        case 8:
          if (contrast < 127) contrast++;
          disp.invertText(true);
          disp.printNumI(contrast, 42, 32, 3);
          disp.invertText(false);
          disp.printNumI(25 - blk_level, 42, 24, 3);
          if (blk_level == 0) disp.print("MAX", 42, 24);
          if (blk_level == 25) disp.print("OFF", 42, 24);
          disp.setContrast(contrast);
          break;
      }
    }
    if (encoder.isRight()) {
      switch (count) {
        case 1:
          if (relay_on) relay_on = false;
          else relay_on = true;
          disp.invertText(true);
          if (relay_on) disp.print("ON ", 36, 16);
          else disp.print("OFF", 36, 16);
          break;
        case 3:
          alarm_hour_on--;
          if (alarm_hour_on > 23) alarm_hour_on = 23;
          disp.invertText(true);
          disp.printNumI(alarm_hour_on, 42, 0, 2);
          if (alarm_hour_on < 10) disp.print("0", 42, 0);
          break;
        case 4:
          alarm_min_on--;
          if (alarm_min_on > 59) alarm_min_on = 59;
          disp.invertText(true);
          disp.printNumI(alarm_min_on, 60, 0, 2);
          if (alarm_min_on < 10) disp.print("0", 60, 0);
          disp.invertText(false);
          disp.printNumI(alarm_hour_on, 42, 0, 2);
          if (alarm_hour_on < 10) disp.print("0", 42, 0);
          break;
        case 5:
          alarm_hour_off--;
          if (alarm_hour_off > 23) alarm_hour_off = 23;
          disp.invertText(true);
          disp.printNumI(alarm_hour_off, 42, 8, 2);
          if (alarm_hour_off < 10) disp.print("0", 42, 8);
          disp.invertText(false);
          disp.printNumI(alarm_min_on, 60, 0, 2);
          if (alarm_min_on < 10) disp.print("0", 60, 0);
          break;
        case 6:
          alarm_min_off--;
          if (alarm_min_off > 59) alarm_min_off = 59;
          disp.invertText(true);
          disp.printNumI(alarm_min_off, 60, 8, 2);
          if (alarm_min_off < 10) disp.print("0", 60, 8);
          disp.invertText(false);
          disp.printNumI(alarm_hour_off, 42, 8, 2);
          if (alarm_hour_off < 10) disp.print("0", 42, 8);
          break;
        case 7:
          if (blk_level < 25) blk_level++;
          disp.invertText(true);
          if (blk_level == 25) {
            disp.print("OFF", 42, 24);
            blk_level_raw = 255;
            analogWrite(pin_display_blk, blk_level_raw);
          }
          else {
            blk_level_raw = blk_level * 10;
            analogWrite(pin_display_blk, blk_level_raw);
            disp.printNumI(25 - blk_level, 42, 24, 3);
          }
          disp.invertText(false);
          break;
        case 8:
          if (contrast > 0) contrast--;
          disp.invertText(true);
          disp.printNumI(contrast, 42, 32, 3);
          disp.invertText(false);
          disp.printNumI(25 - blk_level, 42, 24, 3);
          if (blk_level == 0) disp.print("MAX", 42, 24);
          if (blk_level == 25) disp.print("OFF", 42, 24);
          disp.setContrast(contrast);
          break;
      }
    }
    if (encoder.isHolded() && ok) {
      disp.clrScr();
      blk_time = millis();
      EEPROM.update(contrast_addr, contrast);
      EEPROM.update(blk_level_addr, blk_level);
      EEPROM.update(alarm_hour_on_addr, alarm_hour_on);
      EEPROM.update(alarm_min_on_addr, alarm_min_on);
      EEPROM.update(alarm_hour_off_addr, alarm_hour_off);
      EEPROM.update(alarm_min_off_addr, alarm_min_off);
      EEPROM.update(blk_level_raw_addr, blk_level_raw);
      EEPROM.update(relay_on_addr, relay_on);
      Timer1.attachInterrupt(disp_time);
      return;
    }
    if (encoder.isClick()) {
      count++;
      ok = false;
      if (count > 8) count = 1;
      switch (count) {
        case 1:
          disp.invertText(true);
          if (relay_on) disp.print("ON ", 36, 16);
          else disp.print("OFF", 36, 16);
          disp.invertText(false);
          disp.printNumI(contrast, 42, 32, 3);
          break;
        case 3:
          disp.print("  ", CENTER, 40);
          disp.invertText(true);
          disp.printNumI(alarm_hour_on, 42, 0, 2);
          if (alarm_hour_on < 10) disp.print("0", 42, 0);
          disp.invertText(false);
          if (relay_on) disp.print("ON ", 36, 16);
          else disp.print("OFF", 36, 16);
          break;
        case 4:
          disp.invertText(true);
          disp.printNumI(alarm_min_on, 60, 0, 2);
          if (alarm_min_on < 10) disp.print("0", 60, 0);
          disp.invertText(false);
          disp.printNumI(alarm_hour_on, 42, 0, 2);
          if (alarm_hour_on < 10) disp.print("0", 42, 0);
          break;
        case 5:
          disp.invertText(true);
          disp.printNumI(alarm_hour_off, 42, 8, 2);
          if (alarm_hour_off < 10) disp.print("0", 42, 8);
          disp.invertText(false);
          disp.printNumI(alarm_min_on, 60, 0, 2);
          if (alarm_min_on < 10) disp.print("0", 60, 0);
          break;
        case 6:
          disp.invertText(true);
          disp.printNumI(alarm_min_off, 60, 8, 2);
          if (alarm_min_off < 10) disp.print("0", 60, 8);
          disp.invertText(false);
          disp.printNumI(alarm_hour_off, 42, 8, 2);
          if (alarm_hour_off < 10) disp.print("0", 42, 8);
          break;
        case 2:
          disp.invertText(true);
          disp.print("Ok", CENTER, 40);
          disp.invertText(false);
          if (relay_on) disp.print("Timer ON ", LEFT, 16);
          else disp.print("Timer OFF", LEFT, 16);
          ok = true;
          break;
        case 7:
          disp.print("Ligth:", LEFT, 24);
          disp.invertText(true);
          disp.printNumI(25 - blk_level, 42, 24, 3);
          disp.invertText(false);
          disp.printNumI(alarm_min_off, 60, 8, 2);
          if (alarm_min_off < 10) disp.print("0", 60, 8);
          break;
        case 8:
          disp.print("Contr:", LEFT, 32);
          disp.invertText(true);
          disp.printNumI(contrast, 42, 32, 3);
          disp.invertText(false);
          disp.printNumI(25 - blk_level, 42, 24, 3);
          break;
      }
    }
  }
}

void disp_time() {
  int t_alarm_on, t_now, t_alarm_off;
  boolean zero_hours;
  interrupts();
  DateTime now = rtc.now();
  year = now.year();
  month = now.month();
  day = now.day();
  hour = now.hour();
  min = now.minute();
  if ((hour > 23) && (min > 59)) {
    hour = 0;
    min = 0;
  }
  if (splash) {
    disp.setFont(MediumNumbers);
    disp.print(".", 30 + 4, 0);
    disp.printNumI(hour, 14, 0, 2);
    if (hour < 10) disp.print("0", 14, 0);
    disp.printNumI(min, 30 + 12, 0, 2);
    if (min < 10) disp.print("0", 30 + 12, 0);
    splash = false;
  }
  else {
    disp.setFont(SmallFont);
    disp.print("  ", 30 + 4, 8);
    disp.setFont(MediumNumbers);
    disp.printNumI(hour, 14, 0, 2);
    if (hour < 10) disp.print("0", 14, 0);
    disp.printNumI(min, 30 + 12, 0, 2);
    if (min < 10) disp.print("0", 30 + 12, 0);
    splash = true;
  }

  if (relay_on) {
    t_alarm_on = alarm_hour_on * 60 + alarm_min_on-1;
    t_alarm_off = alarm_hour_off * 60 + alarm_min_off-1;
    t_now = hour * 60 + min;
    if (t_alarm_on > t_alarm_off) zero_hours = true; else zero_hours = false;                                       //проверяем на переход через 00:00
    if (t_now > t_alarm_on) relay_now = true;                                                                       //простое вкючение
    if (zero_hours && (t_now < t_alarm_on) && (t_now < t_alarm_off)) relay_now = true;
    if (((t_now > t_alarm_off) || (t_now < t_alarm_on)) && !zero_hours) relay_now = false;                          //простое выключение
    if (zero_hours && (t_now < t_alarm_on) && (t_now > t_alarm_off)) relay_now = false;                             //выкслюение при флаге 00:00
  }
  else {
    disp.setFont(SmallFont);
    disp.print("Timer OFF", LEFT, 24);
  }
  if (relay_on && !relay_now) {
    disp.setFont(SmallFont);
    disp.print("Relay: Ready", LEFT, 24);
  }
  if (relay_now) {
    disp.setFont(SmallFont);
    disp.print("Relay: ON   ", LEFT, 24);
    disp.print("est:", LEFT, 32);
    disp.printNumI((t_alarm_off - t_now) / 60, 30, 32, 2);
    if ((t_alarm_off - t_now) / 60 < 10) disp.print("0", 30, 32);
    disp.print(":", 42, 32);
    disp.printNumI(((t_alarm_off - t_now) % 60)-1, 48, 32, 2);
    if ((t_alarm_off - t_now) % 60 < 10) disp.print("0", 48, 32);

    digitalWrite(pin_relay, LOW);
  }
  if (relay_on && !relay_now) {
    disp.setFont(SmallFont);
    disp.print("Relay: OFF  ", LEFT, 24);
    disp.print("est:", LEFT, 32);
    if (t_now < t_alarm_off) {
      disp.printNumI((t_alarm_on - t_now) / 60, 30, 32, 2);
      if ((t_alarm_on - t_now) / 60 < 10) disp.print("0", 30, 32);
      disp.print(":", 42, 32);
      disp.printNumI(((t_alarm_on - t_now) % 60)-1, 48, 32, 2);
      if ((t_alarm_on - t_now) % 60 < 10) disp.print("0", 48, 32);
    }
    else {
      disp.printNumI((24 * 60 - t_now + t_alarm_on) / 60, 30, 32, 2);
      if ((24 * 60 - t_now + t_alarm_on) / 60 < 10) disp.print("0", 30, 32);
      disp.print(":", 42, 32);
      disp.printNumI((24 * 60 - t_now + t_alarm_on) % 60, 48, 32, 2);
      if ((24 * 60 - t_now + t_alarm_on) % 60 < 10) disp.print("0", 48, 32);
    }
    digitalWrite(pin_relay, HIGH);
  }
}

void adjust_time() {
  byte count = 1;
  boolean ok = false;
  disp.clrScr();
  analogWrite(pin_display_blk, blk_level_raw);

  disp.print("Date:", 0, 0);
  disp.invertText(true);
  if (year < 2020) year = 2020;
  disp.printNumI(year - 2000, 32, 0, 2);
  disp.invertText(false);
  disp.print("/", 32 + 12, 0);
  disp.printNumI(month, 32 + 18, 0, 2);
  if (month < 10) disp.print("0", 32 + 18, 0);
  disp.print("/", 32 + 30, 0);
  disp.printNumI(day, 32 + 36, 0, 2);
  if (day < 10) disp.print("0", 32 + 36, 0);
  disp.print("Time:", 0, 24);
  disp.printNumI(hour, 32, 24, 2);
  if (hour < 10) disp.print("0", 32, 24);
  disp.print(":", 32 + 12, 24);
  disp.printNumI(min, 32 + 17, 24, 2);
  if (min < 10) disp.print("0", 32 + 17, 24);

  while (true) {
    encoder.tick();
    if (encoder.isLeft()) {
      switch (count) {
        case 1:
          year++;
          if (year < 2020) year = 2020;
          disp.invertText(true);
          disp.printNumI(year - 2000, 32, 0, 2);
          disp.invertText(false);
          break;
        case 2:
          month++;
          if (month > 12) month = 1;
          disp.invertText(true);
          disp.printNumI(month, 32 + 18, 0, 2);
          if (month < 10) disp.print("0", 32 + 18, 0);
          disp.invertText(false);
          break;
        case 3:
          day++;
          if (day > 31) day = 1;
          disp.invertText(true);
          disp.printNumI(day, 32 + 36, 0, 2);
          if (day < 10) disp.print("0", 32 + 36, 0);
          disp.invertText(false);
          break;
        case 4:
          hour++;
          if (hour > 23) hour = 0;
          disp.invertText(true);
          disp.printNumI(hour, 32, 24, 2);
          if (hour < 10) disp.print("0", 32, 24);
          disp.invertText(false);
          break;
        case 5:
          min++;
          if (min > 59) min = 0;
          disp.invertText(true);
          disp.printNumI(min, 32 + 17, 24, 2);
          if (min < 10) disp.print("0", 32 + 17, 24);
          disp.invertText(false);
      }
    }
    if (encoder.isRight()) {
      switch (count) {
        case 1:
          year--;
          if (year < 2020) year = 2020;
          disp.invertText(true);
          disp.printNumI(year - 2000, 32, 0, 2);
          disp.invertText(false);
          break;
        case 2:
          month--;
          if (month < 1) month = 12;
          disp.invertText(true);
          disp.printNumI(month, 32 + 18, 0, 2);
          if (month < 10) disp.print("0", 32 + 18, 0);
          disp.invertText(false);
          break;
        case 3:
          day--;
          if (day > 31) day = 31;
          disp.invertText(true);
          disp.printNumI(day, 32 + 36, 0, 2);
          if (day < 10) disp.print("0", 32 + 36, 0);
          disp.invertText(false);
          break;
        case 4:
          hour--;
          if (hour > 23) hour = 23;
          disp.invertText(true);
          disp.printNumI(hour, 32, 24, 2);
          if (hour < 10) disp.print("0", 32, 24);
          disp.invertText(false);
          break;
        case 5:
          min--;
          if (min > 59) min = 59;
          disp.invertText(true);
          disp.printNumI(min, 32 + 17, 24, 2);
          if (min < 10) disp.print("0", 32 + 17, 24);
          break;
      }
    }
    if (encoder.isHolded() && ok) {
      analogWrite(pin_display_blk, blk_level_raw);
      blk_time = millis();
      disp.clrScr();
      rtc.adjust(DateTime(year, month, day, hour, min));
      Timer1.attachInterrupt(disp_time);
      return;
    }
    if (encoder.isClick()) {
      ok = false;
      count++;
      if (count > 6) count = 1;
      switch (count) {
        case 1:
          if (year < 2020) year = 2020;
          disp.invertText(true);
          disp.printNumI(year - 2000, 32, 0, 2);
          disp.clrRow(5);
          disp.invertText(true);
          break;
        case 2:
          disp.invertText(true);
          disp.printNumI(month, 32 + 18, 0, 2);
          if (month < 10) disp.print("0", 32 + 18, 0);
          disp.invertText(false);
          disp.printNumI(year - 2000, 32, 0, 2);
          disp.invertText(true);
          break;
        case 3:
          disp.invertText(true);
          disp.printNumI(day, 32 + 36, 0, 2);
          if (day < 10) disp.print("0", 32 + 36, 0);
          disp.invertText(false);
          disp.printNumI(month, 32 + 18, 0, 2);
          if (month < 10) disp.print("0", 32 + 18, 0);
          disp.invertText(true);
          break;
        case 4:
          disp.invertText(true);
          disp.printNumI(hour, 32, 24, 2);
          if (hour < 10) disp.print("0", 32, 24);
          disp.invertText(false);
          disp.printNumI(day, 32 + 36, 0, 2);
          if (day < 10) disp.print("0", 32 + 36, 0);
          disp.invertText(true);
          break;
        case 5:
          disp.invertText(true);
          disp.printNumI(min, 32 + 17, 24, 2);
          if (min < 10) disp.print("0", 32 + 17, 24);
          disp.invertText(false);
          disp.printNumI(hour, 32, 24, 2);
          if (hour < 10) disp.print("0", 32, 24);
          disp.invertText(true);
          break;
        case 6:
          disp.invertText(true);
          disp.print("Ok", 32, 40);
          disp.invertText(false);
          disp.printNumI(min, 32 + 17, 24, 2);
          if (min < 10) disp.print("0", 32 + 17, 24);
          disp.invertText(true);
          ok = true;
      }
    }
  }
}
