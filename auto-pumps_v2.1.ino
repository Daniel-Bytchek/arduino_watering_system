#define LCD_BACKL 1          
#define BACKL_TOUT 60       
#define ENCODER_TYPE 1      
#define ENC_REVERSE 1       
#define DRIVER_VERSION 0    
#define PUPM_AMOUNT 8       
#define START_PIN 4         
#define SWITCH_LEVEL 0      
#define PARALLEL 0         
#define TIMER_START 1       

static const wchar_t *relayNames[]  = {
  L"Помпа №1",
  L"Помпа №2",
  L"Помпа №3",
  L"Помпа №4",
};

#define CLK 3
#define DT 2
#define SW 0

#include "Encoder.h"
Encoder enc1(CLK, DT, SW);

#include <EEPROMex.h>
#include <EEPROMVar.h>

#include "LCD_1602_UA.h"

#if (DRIVER_VERSION)
LCD_1602_UA lcd(0x27, 16, 2);
#else
LCD_1602_UA lcd(0x3f, 16, 2);
#endif

uint32_t pump_timers[PUPM_AMOUNT];
uint32_t pumping_time[PUPM_AMOUNT];
uint32_t period_time[PUPM_AMOUNT];
boolean pump_state[PUPM_AMOUNT];
byte pump_pins[PUPM_AMOUNT];

int8_t current_set;
int8_t current_pump;
boolean now_pumping;

int8_t thisH, thisM, thisS;
long thisPeriod;
boolean startFlag = true;

boolean backlState = true;
uint32_t backlTimer;

void setup() {

  for (byte i = 0; i < PUPM_AMOUNT; i++) {            
    pump_pins[i] = START_PIN + i;                   
    pinMode(START_PIN + i, OUTPUT);                 
    digitalWrite(START_PIN + i, !SWITCH_LEVEL);     
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  enc1.setType(ENCODER_TYPE);
  if (ENC_REVERSE) enc1.setDirection(REVERSE);


  if (!digitalRead(SW)) {      
    lcd.setCursor(0, 0);
    lcd.print("Reset settings");
    for (byte i = 0; i < 500; i++) {
      EEPROM.writeLong(i, 0);
    }
  }
  while (!digitalRead(SW));       
  lcd.clear();                    

  if (EEPROM.read(1023) != 5) {
    EEPROM.writeByte(1023, 5);

    for (byte i = 0; i < 500; i += 4) {
      EEPROM.writeLong(i, 0);
    }
  }

  for (byte i = 0; i < PUPM_AMOUNT; i++) {           
    period_time[i] = EEPROM.readLong(8 * i);     
    pumping_time[i] = EEPROM.readLong(8 * i + 4);    

    if (SWITCH_LEVEL)		
      pump_state[i] = 0;
    else
      pump_state[i] = 1;
  }

  drawLabels();
  changeSet();
}

void loop() {
  encoderTick();
  periodTick();
  flowTick();
  backlTick();
}

void backlTick() {
  if (LCD_BACKL && backlState && millis() - backlTimer >= BACKL_TOUT * 1000) {
    backlState = false;
    lcd.noBacklight();
  }
}
void backlOn() {
  backlState = true;
  backlTimer = millis();
  lcd.backlight();
}

void periodTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {            
    if (startFlag ||
        (period_time[i] > 0
         && millis() - pump_timers[i] >= period_time[i] * 1000
         && (pump_state[i] != SWITCH_LEVEL)
         && !(now_pumping * !PARALLEL))) {
      pump_state[i] = SWITCH_LEVEL;
      digitalWrite(pump_pins[i], SWITCH_LEVEL);
      pump_timers[i] = millis();
      now_pumping = true;
      //Serial.println("Pump #" + String(i) + " ON");
    }
  }
  startFlag = false;
}

void flowTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {          
    if (pumping_time[i] > 0
        && millis() - pump_timers[i] >= pumping_time[i] * 1000
        && (pump_state[i] == SWITCH_LEVEL) ) {
      pump_state[i] = !SWITCH_LEVEL;
      digitalWrite(pump_pins[i], !SWITCH_LEVEL);
      if (TIMER_START) pump_timers[i] = millis();
      now_pumping = false;
      //Serial.println("Pump #" + String(i) + " OFF");
    }
  }
}

void encoderTick() {
  enc1.tick();    

  if (enc1.isTurn()) {                             
    if (backlState) {
      backlTimer = millis();     
      if (enc1.isRight()) {
        if (++current_set >= 7) current_set = 6;
      } else if (enc1.isLeft()) {
        if (--current_set < 0) current_set = 0;
      }

      if (enc1.isRightH())
        changeSettings(1);
      else if (enc1.isLeftH())
        changeSettings(-1);

      changeSet();
    } else {
      backlOn();    
    }
  }
}

void changeSettings(int increment) {
  if (current_set == 0) {
    current_pump += increment;
    if (current_pump > PUPM_AMOUNT - 1) current_pump = PUPM_AMOUNT - 1;
    if (current_pump < 0) current_pump = 0;
    s_to_hms(period_time[current_pump]);
    drawLabels();
  } else {
    if (current_set == 1 || current_set == 4) {
      thisH += increment;
    } else if (current_set == 2 || current_set == 5) {
      thisM += increment;
    } else if (current_set == 3 || current_set == 6) {
      thisS += increment;
    }
    if (thisS > 59) {
      thisS = 0;
      thisM++;
    }
    if (thisM > 59) {
      thisM = 0;
      thisH++;
    }
    if (thisS < 0) {
      if (thisM > 0) {
        thisS = 59;
        thisM--;
      } else thisS = 0;
    }
    if (thisM < 0) {
      if (thisH > 0) {
        thisM = 59;
        thisH--;
      } else thisM = 0;
    }
    if (thisH < 0) thisH = 0;
    if (current_set < 4) period_time[current_pump] = hms_to_s();
    else pumping_time[current_pump] = hms_to_s();
  }
}

void drawLabels() {
  lcd.setCursor(1, 0);
  lcd.print("                ");
  lcd.setCursor(1, 0);
  lcd.print(relayNames[current_pump]);
}

void changeSet() {
  switch (current_set) {
    case 0: drawArrow(0, 0); update_EEPROM();
      break;
    case 1: drawArrow(7, 1);
      break;
    case 2: drawArrow(10, 1);
      break;
    case 3: drawArrow(13, 1);
      break;
    case 4: drawArrow(7, 1);
      break;
    case 5: drawArrow(10, 1);
      break;
    case 6: drawArrow(13, 1);
      break;
  }
  lcd.setCursor(0, 1);
  if (current_set < 4) {
    lcd.print(L"ПАУЗА ");
    s_to_hms(period_time[current_pump]);
  }
  else {
    lcd.print(L"РОБОТА");
    s_to_hms(pumping_time[current_pump]);
  }
  lcd.setCursor(8, 1);
  if (thisH < 10) lcd.print(0);
  lcd.print(thisH);
  lcd.setCursor(11, 1);
  if (thisM < 10) lcd.print(0);
  lcd.print(thisM);
  lcd.setCursor(14, 1);
  if (thisS < 10) lcd.print(0);
  lcd.print(thisS);
}


void s_to_hms(uint32_t period) {
  thisH = floor((long)period / 3600);   
  thisM = floor((period - (long)thisH * 3600) / 60);
  thisS = period - (long)thisH * 3600 - thisM * 60;
}

uint32_t hms_to_s() {
  return ((long)thisH * 3600 + thisM * 60 + thisS);
}

void drawArrow(byte col, byte row) {
  lcd.setCursor(0, 0); lcd.print(" ");
  lcd.setCursor(7, 1); lcd.print(" ");
  lcd.setCursor(10, 1); lcd.print(":");
  lcd.setCursor(13, 1); lcd.print(":");
  lcd.setCursor(col, row); lcd.write(126);
}

void update_EEPROM() {
  EEPROM.updateLong(8 * current_pump, period_time[current_pump]);
  EEPROM.updateLong(8 * current_pump + 4, pumping_time[current_pump]);
}
