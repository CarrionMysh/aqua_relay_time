#define pin_enc_clk A0
#define pin_enc_dt A1
#define pin_enc_sw A2
#define pin_display_clk 8
#define pin_display_din 9
#define pin_display_ce 12
#define pin_display_rst 11
#define pin_display_dc 10
#define pin_display_blk 5
#define pin_ds18b20 4
#define pin_button1 2
#define pin_button2 3

#include "RTClib.h"
#include <LCD5110_Basic.h>
#include "GyverEncoder.h"
#include <EEPROM.h>
#include <TimerOne.h>
#include <OneWire.h>
#include <DallasTemperature.h>

int alarm_hour_on_addr[2] = {0, 1};
int alarm_min_on_addr[2] = {2, 3};
int alarm_hour_off_addr[2] = {4, 5};
int alarm_min_off_addr[2] = {6, 7};
int contrast_addr[2] = {8, 9};
int blk_level_addr[2] = {10, 11};
int blk_level_raw_addr[2] = {12, 13};
int relay_on_addr[2] = {14, 15};

volatile unsigned int year;
volatile byte month;
volatile byte day;
volatile byte hour;
volatile byte min;
volatile boolean relay_on[2];
volatile boolean splash;
volatile boolean relay_now[2];
volatile boolean push_buttons;
volatile float temperature;
byte pin_relay[2] = {6, 7};
byte alarm_hour_on[2];
byte alarm_min_on[2];
byte alarm_hour_off[2];
byte alarm_min_off[2];
byte ds_addr[8];
byte contrast;
byte blk_level;
byte blk_level_raw;
unsigned long blk_time;
unsigned long blk_timeout = 3000;
unsigned int debounce_button = 90;
volatile unsigned long time_push_button1 = 0;
volatile unsigned long time_push_button2 = 0;
int ds_resolution = 9;

Encoder encoder(pin_enc_clk, pin_enc_dt, pin_enc_sw);
RTC_DS1307 rtc;
LCD5110 disp(pin_display_clk, pin_display_din, pin_display_dc, pin_display_rst, pin_display_ce);
OneWire ds_bus(pin_ds18b20);
DallasTemperature ds(&ds_bus);
DeviceAddress ds_dev_addr;

extern uint8_t SmallFont[];
extern uint8_t MediumNumbers[];

void setup() {
  for (byte n_relay = 0; n_relay <= 1; n_relay++) {
    pinMode(pin_relay[n_relay], OUTPUT);
    digitalWrite(pin_relay[n_relay], HIGH);
    pinMode(pin_button1, INPUT);
    pinMode(pin_button2, INPUT);
    digitalWrite(pin_button1, HIGH);
    digitalWrite(pin_button2, HIGH);
    alarm_hour_on[n_relay] = EEPROM.read(alarm_hour_on_addr[n_relay]);
    alarm_min_on[n_relay] = EEPROM.read(alarm_min_on_addr[n_relay]);
    alarm_hour_off[n_relay] = EEPROM.read(alarm_hour_off_addr[n_relay]);
    alarm_min_off[n_relay] = EEPROM.read(alarm_min_off_addr[n_relay]);
    relay_on[n_relay] = EEPROM.read(relay_on_addr[n_relay]);
  }
  pinMode(pin_display_blk, OUTPUT);
  contrast = EEPROM.read(contrast_addr);
  blk_level = EEPROM.read(blk_level_addr);
  blk_level_raw = EEPROM.read(blk_level_raw_addr);
  contrast = 60;
  disp.InitLCD();
  disp.setFont(SmallFont);
  disp.setContrast(contrast);
  analogWrite(pin_display_blk, blk_level_raw);
  encoder.setType(TYPE2);
  Timer1.initialize(500000);
  attachInterrupt(0, button1, LOW);
  attachInterrupt(1, button2, LOW);

  ds.begin();
  ds.getAddress(ds_dev_addr, 0);
  ds.setResolution(ds_dev_addr, ds_resolution);
  ds.setWaitForConversion(false);
  ds.requestTemperatures();

  if (!rtc.begin()) {
    disp.clrScr();
    disp.print("RTC module failed", CENTER, 24);
    abort();
  }
  if (!rtc.isrunning()) {
    disp.clrScr();
    disp.print("RTC is", CENTER, 8);
    disp.print("not running", CENTER, 16);
    disp.print("pls set", CENTER, 24);
    disp.print("date/time", CENTER, 32);
    while (true) {
      encoder.tick();
      if (encoder.isClick() || encoder.isTurn()) break;
    }
    adjust_time();
    disp.invertText(false);
    disp.clrScr();
  }
  attachInterrupt(0, button1, LOW);
  attachInterrupt(1, button2, LOW);
  Timer1.attachInterrupt(disp_time);
}

void loop() {
  encoder.tick();
  if (encoder.isClick()) {
    Timer1.detachInterrupt();
    menu();
    blk_time = millis();

  }
  if (encoder.isHolded()) {
    Timer1.detachInterrupt();
    adjust_time();
    blk_time = millis();

  }
  if (push_buttons) {
    push_buttons = false;
    blk_time = millis();
    analogWrite(pin_display_blk, blk_level_raw);
  }
  if ((millis() - blk_time) > blk_timeout) analogWrite(pin_display_blk, 255);
}

void menu() {
  byte count = 0;
  boolean ok = false;
  boolean ok_add = false;
  boolean flag_isHolded = false;
  byte n_relay = 0;

  analogWrite(pin_display_blk, blk_level_raw);
  disp.setFont(SmallFont);
  disp.clrScr();

  disp.print("Timer", LEFT, 0);
  if (relay_on[0]) disp.print("ON ", 36, 0);
  else disp.print("OFF", 36, 0);
  if (relay_on[1]) disp.print("ON ", 66, 0);
  else disp.print("OFF", 66, 0);

  disp.print("On1 :", LEFT, 8);
  disp.printNumI(alarm_hour_on[0], 42, 8, 2, '0');
  disp.print(".", 54, 8);
  disp.printNumI(alarm_min_on[0], 60, 8, 2, '0');

  disp.print("Off1: ", LEFT, 16);
  disp.printNumI(alarm_hour_off[0], 42, 16, 2, '0');
  disp.print(".", 54, 16);
  disp.printNumI(alarm_min_off[0], 60, 16, 2, '0');

  disp.print("On2 :", LEFT, 24);
  disp.printNumI(alarm_hour_on[1], 42, 24, 2, '0');
  disp.print(".", 54, 24);
  disp.printNumI(alarm_min_on[1], 60, 24, 2, '0');

  disp.print("Off2: ", LEFT, 32);
  disp.printNumI(alarm_hour_off[1], 42, 32, 2, '0');
  disp.print(".", 54, 32);
  disp.printNumI(alarm_min_off[1], 60, 32, 2, '0');

  while (true) {
    flag_isHolded = false;
    encoder.tick();
    if (encoder.isLeft()) {
      switch (count) {
        case 1:                                                             //таймер1 on/off
          if (relay_on[0]) relay_on[0] = false;
          else relay_on[0] = true;
          disp.invertText(true);
          if (relay_on[0]) disp.print("ON ", 36, 0);
          else disp.print("OFF", 36, 0);
          break;
        case 2:                                                        //таймер2  on/off
          if (relay_on[1]) relay_on[1] = false;
          else relay_on[1] = true;
          disp.invertText(true);
          if (relay_on[1]) disp.print("ON ", 66, 0);
          else disp.print("OFF", 66, 0);
          break;
        case 3:                                                         //on hour 1
          alarm_hour_on[0]++;
          if (alarm_hour_on[0] > 23) alarm_hour_on[0] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_hour_on[0], 42, 8, 2, '0');
          break;
        case 4:                                                         //on min 1
          alarm_min_on[0]++;
          if (alarm_min_on[0] > 59) alarm_min_on[0] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_min_on[0], 60, 8, 2, '0');
          break;
        case 5:                                                         //off hour 1
          alarm_hour_off[0]++;
          if (alarm_hour_off[0] > 23) alarm_hour_off[0] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_hour_off[0], 42, 16, 2, '0');
          break;
        case 6:                                                         //off min 1
          alarm_min_off[0]++;
          if (alarm_min_off[0] > 59) alarm_min_off[0] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_min_off[0], 60, 16, 2, '0');
          break;
        case 7:                                                         //on hour 2
          alarm_hour_on[1]++;
          if (alarm_hour_on[1] > 23) alarm_hour_on[1] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_hour_on[1], 42, 24, 2, '0');
          break;
        case 8:                                                         //on min 2
          alarm_min_on[1]++;
          if (alarm_min_on[1] > 59) alarm_min_on[1] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_min_on[1], 60, 24, 2, '0');
          break;
        case 9:                                                          //off hour 2
          alarm_hour_off[1]++;
          if (alarm_hour_off[1] > 23) alarm_hour_off[1] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_hour_off[1], 42, 32, 2, '0');
          break;
        case 10:                                                        //off min 2
          alarm_min_off[1]++;
          if (alarm_min_off[1] > 59) alarm_min_off[1] = 0;
          disp.invertText(true);
          disp.printNumI(alarm_min_off[1], 60, 32, 2, '0');
          break;
        case 11:                                                         //ok/adding menu
          if (ok) {
            ok = false;
            ok_add = true;
          }
          else {
            ok = true;
            ok_add = false;
          }
          disp.invertText(true);
          disp.print("        ", CENTER, 40);
          if (ok_add) disp.print("settings", CENTER, 40);
          if (ok) disp.print("Ok", CENTER, 40);
          break;
      }
    }
    if (encoder.isRight()) {
      switch (count) {
        case 1:                                                             //таймер1 on/off
          if (relay_on[0]) relay_on[0] = false;
          else relay_on[0] = true;
          disp.invertText(true);
          if (relay_on[0]) disp.print("ON ", 36, 0);
          else disp.print("OFF", 36, 0);
          break;
        case 2:                                                        //таймер2  on/off
          if (relay_on[1]) relay_on[1] = false;
          else relay_on[1] = true;
          disp.invertText(true);
          if (relay_on[1]) disp.print("ON ", 66, 0);
          else disp.print("OFF", 66, 0);
          break;
        case 3:                                                         //on hour 1
          alarm_hour_on[0]--;
          if (alarm_hour_on[0] > 23) alarm_hour_on[0] = 23;
          disp.invertText(true);
          disp.printNumI(alarm_hour_on[0], 42, 8, 2, '0');
          break;
        case 4:                                                         //on min 1
          alarm_min_on[0]--;
          if (alarm_min_on[0] > 59) alarm_min_on[0] = 59;
          disp.invertText(true);
          disp.printNumI(alarm_min_on[0], 60, 8, 2, '0');
          break;
        case 5:                                                         //off hour 1
          alarm_hour_off[0]--;
          if (alarm_hour_off[0] > 23) alarm_hour_off[0] = 23;
          disp.invertText(true);
          disp.printNumI(alarm_hour_off[0], 42, 16, 2, '0');
          break;
        case 6:                                                         //off min 1
          alarm_min_off[0]--;
          if (alarm_min_off[0] > 59) alarm_min_off[0] = 59;
          disp.invertText(true);
          disp.printNumI(alarm_min_off[0], 60, 16, 2, '0');
          break;;
        case 7:                                                         //on hour 2
          alarm_hour_on[1]--;
          if (alarm_hour_on[1] > 23) alarm_hour_on[1] = 23;
          disp.invertText(true);
          disp.printNumI(alarm_hour_on[1], 42, 24, 2, '0');
          break;
        case 8:                                                         //on min 2
          alarm_min_on[1]--;
          if (alarm_min_on[1] > 59) alarm_min_on[1] = 59;
          disp.invertText(true);
          disp.printNumI(alarm_min_on[1], 60, 24, 2, '0');
          break;
        case 9:                                                          //off hour 2
          alarm_hour_off[1]--;
          if (alarm_hour_off[1] > 23) alarm_hour_off[1] = 23;
          disp.invertText(true);
          disp.printNumI(alarm_hour_off[1], 42, 32, 2, '0');
          break;
        case 10:                                                        //off min 2
          alarm_min_off[1]--;
          if (alarm_min_off[1] > 59) alarm_min_off[1] = 59;
          disp.invertText(true);
          disp.printNumI(alarm_min_off[1], 60, 32, 2, '0');
          break;
        case 11:                                                         //ok/adding menu
          if (ok) {
            ok = false;
            ok_add = true;
          }
          else {
            ok = true;
            ok_add = false;
          }
          disp.invertText(true);
          disp.print("        ", CENTER, 40);
          if (ok_add) disp.print("settings", CENTER, 40);
          if (ok) disp.print("Ok", CENTER, 40);
          break;
      }
    }
    if (encoder.isHolded()) flag_isHolded = true;
    if (flag_isHolded && ok) {
      disp.clrScr();
      for (byte n_relay = 0; n_relay <= 1; n_relay++) {
        EEPROM.update(alarm_hour_on_addr[n_relay], alarm_hour_on[n_relay]);
        EEPROM.update(alarm_min_on_addr[n_relay], alarm_min_on[n_relay]);
        EEPROM.update(alarm_hour_off_addr[n_relay], alarm_hour_off[n_relay]);
        EEPROM.update(alarm_min_off_addr[n_relay], alarm_min_off[n_relay]);
        EEPROM.update(relay_on_addr[n_relay], relay_on[n_relay]);
      }
      Timer1.attachInterrupt(disp_time);
      return;
    }
    if (flag_isHolded && ok_add) {
      adding_menu();
      return;
    }
    if (encoder.isClick()) {
      count++;
      ok = false;
      ok_add = false;
      if (count > 11) count = 1;
      switch (count) {
        case 1:                                                         //таймер1  on/off
          ok = true;
          disp.invertText(true);
          if (relay_on[0]) disp.print("ON ", 36, 0);
          else disp.print("OFF", 36, 0);
          disp.invertText(false);
          disp.print("        ", CENTER, 40);
          break;
        case 2:                                                        //таймер2  on/off
          ok = true;
          disp.invertText(true);
          if (relay_on[1]) disp.print("ON ", 66, 0);
          else disp.print("OFF", 66, 0);
          disp.invertText(false);
          if (relay_on[0]) disp.print("ON ", 36, 0);
          else disp.print("OFF", 36, 0);
          break;
        case 3:                                                         //on hour 1
          disp.invertText(true);
          disp.printNumI(alarm_hour_on[0], 42, 8, 2, '0');
          disp.invertText(false);
          if (relay_on[1]) disp.print("ON ", 66, 0);
          else disp.print("OFF", 66, 0);
          break;
        case 4:                                                         //on min 1
          disp.invertText(true);
          disp.printNumI(alarm_min_on[0], 60, 8, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_hour_on[0], 42, 8, 2, '0');
          break;
        case 5:                                                         //off hour 1
          disp.invertText(true);
          disp.printNumI(alarm_hour_off[0], 42, 16, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_min_on[0], 60, 8, 2, '0');
          break;
        case 6:                                                         //off min 1
          disp.invertText(true);
          disp.printNumI(alarm_min_off[0], 60, 16, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_hour_off[0], 42, 16, 2, '0');
          break;
        case 7:                                                         //on hour 2
          disp.invertText(true);
          disp.printNumI(alarm_hour_on[1], 42, 24, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_min_off[0], 60, 16, 2, '0');
          break;
        case 8:                                                         //on min 2
          disp.invertText(true);
          disp.printNumI(alarm_min_on[1], 60, 24, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_hour_on[1], 42, 24, 2, '0');
          break;
        case 9:                                                          //off hour 2
          disp.invertText(true);
          disp.printNumI(alarm_hour_off[1], 42, 32, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_min_on[1], 60, 24, 2, '0');
          break;
        case 10:                                                        //off min 2
          disp.invertText(true);
          disp.printNumI(alarm_min_off[1], 60, 32, 2, '0');
          disp.invertText(false);
          disp.printNumI(alarm_hour_off[1], 42, 32, 2, '0');
          break;
        case 11:                                                         //ok
          disp.invertText(true);
          disp.print("   Ok   ", CENTER, 40);
          disp.invertText(false);
          disp.printNumI(alarm_min_off[1], 60, 32, 2, '0');
          ok = true;
          break;
      }
    }
  }
}

void disp_time() {
  int t_alarm_on[2], t_now, t_alarm_off[2];
  boolean zero_hours[2];                                                  //признак перехода через полночь
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

  temperature = ds.getTempCByIndex(0);
  ds.requestTemperatures();
  disp.setFont(SmallFont);
  disp.printNumI(hour, 0, 0, 2, '0');
  if (splash) {
    disp.print(":", 12, 0);
    splash = false;
  }
  else {
    disp.print(" ", 12, 0);
    splash = true;
  }
  disp.printNumI(min, 18, 0, 2, '0');

  disp.setFont(MediumNumbers);
  disp.printNumF(temperature, 1, 36, 0);
  disp.setFont(SmallFont);
  t_now = hour * 60 + min;
  Serial.print("t_now: "); Serial.println(t_now);
  for (byte n_relay = 0; n_relay <= 1; n_relay++) {
    if (relay_on[n_relay]) {                                                //проверка по времени актуальность состояния канала реле
      t_alarm_on[n_relay] = alarm_hour_on[n_relay] * 60 + alarm_min_on[n_relay];
      t_alarm_off[n_relay] = alarm_hour_off[n_relay] * 60 + alarm_min_off[n_relay];

      if (t_alarm_on[n_relay] > t_alarm_off[n_relay]) zero_hours[n_relay] = true; else zero_hours[n_relay] = false;                     //проверяем на переход через 00:00
      if (t_now >= t_alarm_on[n_relay]) relay_now[n_relay] = true;                                                                       //простое вкючение
      if (zero_hours[n_relay] && (t_now <= t_alarm_on[n_relay]) && (t_now < t_alarm_off[n_relay])) relay_now[n_relay] = true;
      if (((t_now > t_alarm_off[n_relay]) || (t_now < t_alarm_on[n_relay])) && !zero_hours[n_relay]) relay_now[n_relay] = false;        //простое выключение
      if (zero_hours[n_relay] && (t_now < t_alarm_on[n_relay]) && (t_now > t_alarm_off[n_relay])) relay_now[n_relay] = false;           //выкслюение при флаге 00:00
    }
    else {
      disp.setFont(SmallFont);

      switch (n_relay) {
        case 0:
          disp.print("Timer1", LEFT, 16);
          disp.print("OFF ", 48, 16);
          disp.print("              ", 0, 24);
          break;
        case 1:
          disp.print("Timer2", LEFT, 32);
          disp.print("OFF ", 48, 32);
          disp.print("              ", 0, 40);
          break;
      }
      digitalWrite(pin_relay[n_relay], HIGH);
    }

    if (relay_now[n_relay] && relay_on[n_relay]) {
      disp.setFont(SmallFont);
      switch (n_relay) {
        case 0:
          disp.print("Relay1:", LEFT, 16);
          disp.invertText(true);
          disp.print("ON", 48, 16);
          disp.invertText(false);
          disp.print("  ", 60, 16);
          disp.print("est1:", LEFT, 24);
          //est:
          if (t_now < t_alarm_off[n_relay]) {
            disp.printNumI((t_alarm_off[n_relay] - t_now) / 60, 48, 24, 2, '0');
            disp.print(":", 60, 24);
            disp.printNumI(((t_alarm_off[n_relay] - t_now) % 60) , 66, 24, 2, '0');
          } else {
            disp.printNumI((24 * 60 - t_now + t_alarm_off[n_relay]) / 60, 48, 24 , 2, '0');
            disp.print(":", 60, 24);
            disp.printNumI((24 * 60 - t_now + t_alarm_off[n_relay]) % 60 , 66, 24, 2, '0');
          }
          break;
        case 1:
          disp.print("Relay2:", LEFT, 32);
          disp.print("ON  ", 48, 32);
          disp.print("est2:", LEFT, 40);
          //est:
          disp.printNumI((t_alarm_off[n_relay] - t_now) / 60, 48, 40, 2, '0');
          disp.print(":", 60, 40);
          disp.printNumI(((t_alarm_off[n_relay] - t_now) % 60) , 66, 40, 2, '0');
          break;
      }
      digitalWrite(pin_relay[n_relay], LOW);
    }

    if (relay_on[n_relay] && !relay_now[n_relay]) {
      disp.setFont(SmallFont);
      switch (n_relay) {
        case 0:
          disp.print("Relay1:", LEFT, 16);
          disp.print("WAIT", 48, 16);
          disp.print("est1:", LEFT, 24);
          //est:
          if (t_now < t_alarm_on[n_relay]) {
            disp.printNumI((t_alarm_on[n_relay] - t_now) / 60, 48, 24, 2, '0');
            disp.print(":", 60, 24);
            disp.printNumI(((t_alarm_on[n_relay] - t_now) % 60), 66, 24, 2, '0');
          }
          else {
            disp.printNumI((24 * 60 - t_now + t_alarm_on[n_relay]) / 60, 48, 24, 2, '0');
            disp.print(":", 60, 24);
            disp.printNumI((24 * 60 - t_now + t_alarm_on[n_relay]) % 60, 66, 24, 2, '0');
          }
          break;
        case 1:
          disp.print("Relay2:", LEFT, 32);
          disp.print("WAIT", 48, 32);
          disp.print("est2:", LEFT, 40);
          //est:
          if (t_now < t_alarm_off[n_relay]) {
            disp.printNumI((t_alarm_on[n_relay] - t_now) / 60, 48, 40, 2, '0');
            disp.print(":", 60, 40);
            disp.printNumI(((t_alarm_on[n_relay] - t_now) % 60), 66, 40, 2, '0');
          }
          else {
            disp.printNumI((24 * 60 - t_now + t_alarm_on[n_relay]) / 60, 48, 40, 2, '0');
            disp.print(":", 60, 40);
            disp.printNumI((24 * 60 - t_now + t_alarm_on[n_relay]) % 60, 66, 40, 2, '0');
          }
          break;
      }
      digitalWrite(pin_relay[n_relay], HIGH);
    }
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
  disp.printNumI(month, 32 + 18, 0, 2, '0');
  disp.print("/", 32 + 30, 0);
  disp.printNumI(day, 32 + 36, 0, 2, '0');
  disp.print("Time:", 0, 24);
  disp.printNumI(hour, 32, 24, 2, '0');
  disp.print(":", 32 + 12, 24);
  disp.printNumI(min, 32 + 17, 24, 2, '0');

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
          disp.printNumI(month, 32 + 18, 0, 2, '0');
          disp.invertText(false);
          break;
        case 3:
          day++;
          if (day > 31) day = 1;
          disp.invertText(true);
          disp.printNumI(day, 32 + 36, 0, 2, '0');
          disp.invertText(false);
          break;
        case 4:
          hour++;
          if (hour > 23) hour = 0;
          disp.invertText(true);
          disp.printNumI(hour, 32, 24, 2, '0');
          disp.invertText(false);
          break;
        case 5:
          min++;
          if (min > 59) min = 0;
          disp.invertText(true);
          disp.printNumI(min, 32 + 17, 24, 2, '0');
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
          disp.printNumI(month, 32 + 18, 0, 2, '0');
          disp.invertText(false);
          break;
        case 3:
          day--;
          if (day > 31) day = 31;
          disp.invertText(true);
          disp.printNumI(day, 32 + 36, 0, 2, '0');
          disp.invertText(false);
          break;
        case 4:
          hour--;
          if (hour > 23) hour = 23;
          disp.invertText(true);
          disp.printNumI(hour, 32, 24, 2, '0');
          disp.invertText(false);
          break;
        case 5:
          min--;
          if (min > 59) min = 59;
          disp.invertText(true);
          disp.printNumI(min, 32 + 17, 24, 2, '0');
          break;
      }
    }
    if (encoder.isHolded() && ok) {
      analogWrite(pin_display_blk, blk_level_raw);
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
          disp.printNumI(month, 32 + 18, 0, 2, '0');
          disp.invertText(false);
          disp.printNumI(year - 2000, 32, 0, 2);
          disp.invertText(true);
          break;
        case 3:
          disp.invertText(true);
          disp.printNumI(day, 32 + 36, 0, 2, '0');
          disp.invertText(false);
          disp.printNumI(month, 32 + 18, 0, 2, '0');
          disp.invertText(true);
          break;
        case 4:
          disp.invertText(true);
          disp.printNumI(hour, 32, 24, 2, '0');
          disp.invertText(false);
          disp.printNumI(day, 32 + 36, 0, 2, '0');
          disp.invertText(true);
          break;
        case 5:
          disp.invertText(true);
          disp.printNumI(min, 32 + 17, 24, 2, '0');
          disp.invertText(false);
          disp.printNumI(hour, 32, 24, 2, '0');
          disp.invertText(true);
          break;
        case 6:
          disp.invertText(true);
          disp.print("Ok", 32, 40);
          disp.invertText(false);
          disp.printNumI(min, 32 + 17, 24, 2, '0');
          disp.invertText(true);
          ok = true;
      }
    }
  }
}

void adding_menu() {
  byte count = 0;
  boolean ok = false;
  disp.clrScr();
  disp.invertText(false);
  disp.print("Ligth:", LEFT, 0);
  disp.printNumI(25 - blk_level, 42, 0, 3);
  disp.print("Contr:", LEFT, 8);
  disp.printNumI(contrast, 42, 8, 3);

  while (true) {
    encoder.tick();
    if (encoder.isClick()) {
      count++;
      ok = false;
      if (count > 3) count = 1;
      switch (count) {
        case 1:
          disp.invertText(true);
          disp.printNumI(25 - blk_level, 42, 0, 3);
          disp.invertText(false);
          disp.print("        ", CENTER, 16);
          break;
        case 2:
          disp.invertText(true);
          disp.printNumI(contrast, 42, 8, 3);
          disp.invertText(false);
          disp.printNumI(25 - blk_level, 42, 0, 3);
          break;
        case 3:
          disp.invertText(true);
          disp.print("        ", CENTER, 16);
          disp.print("Ok", CENTER, 16);
          disp.invertText(false);
          disp.printNumI(contrast, 42, 8, 3);
          ok = true;
          break;
      }

    }
    if (encoder.isLeft()) {
      switch (count) {
        case 1:
          if (blk_level > 0) blk_level--;
          disp.invertText(true);
          disp.printNumI(25 - blk_level, 42, 0, 3);
          if (blk_level == 0) {
            disp.print("MAX", 42, 0);
            blk_level_raw = 0;
            analogWrite(pin_display_blk, blk_level_raw);
          } else {
            blk_level_raw = blk_level * 10;
            analogWrite(pin_display_blk, blk_level_raw);
          }
          break;
        case 2:
          if (contrast < 127) contrast++;
          disp.invertText(true);
          disp.printNumI(contrast, 42, 8, 3);
          disp.setContrast(contrast);
          break;
      }
    }
    if (encoder.isRight()) {
      switch (count) {
        case 1:
          if (blk_level < 25) blk_level++;
          disp.invertText(true);
          if (blk_level == 25) {
            disp.print("OFF", 42, 0);
            blk_level_raw = 255;
            analogWrite(pin_display_blk, blk_level_raw);
          }
          else {
            blk_level_raw = blk_level * 10;
            analogWrite(pin_display_blk, blk_level_raw);
            disp.printNumI(25 - blk_level, 42, 0, 3);
          }
          break;
        case 2:
          if (contrast > 0) contrast--;
          disp.invertText(true);
          disp.printNumI(contrast, 42, 8, 3);
          disp.setContrast(contrast);
          break;
      }
    }
    if (encoder.isHolded()) {
      disp.clrScr();
      EEPROM.update(contrast_addr, contrast);
      EEPROM.update(blk_level_addr, blk_level);
      EEPROM.update(blk_level_raw_addr, blk_level_raw);
      for (byte n_relay = 0; n_relay <= 1; n_relay++) {
        EEPROM.update(alarm_hour_on_addr[n_relay], alarm_hour_on[n_relay]);
        EEPROM.update(alarm_min_on_addr[n_relay], alarm_min_on[n_relay]);
        EEPROM.update(alarm_hour_off_addr[n_relay], alarm_hour_off[n_relay]);
        EEPROM.update(alarm_min_off_addr[n_relay], alarm_min_off[n_relay]);
        EEPROM.update(relay_on_addr[n_relay], relay_on[n_relay]);
      }
      Timer1.attachInterrupt(disp_time);
      return;
    }
  }
}

void button1() {
  if ((millis() - time_push_button1) > debounce_button) {
    if (relay_on[0] == true) relay_on[0] = false; else relay_on[0] = true;
    time_push_button1 = millis();
    push_buttons = true;
  }
}

void button2() {
  if ((millis() - time_push_button2) > debounce_button) {
    if (relay_on[1] == true) relay_on[1] = false; else relay_on[1] = true;
    time_push_button2 = millis();
    push_buttons = true;
  }
}
