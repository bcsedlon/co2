#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2017-02-24 14:20:39

#include "Arduino.h"
#include "Arduino.h"
#include <avr/wdt.h>
#include "libraries/Keypad_I2C/Keypad_I2C.h"
#include "libraries/Keypad/Keypad.h"
#include "libraries/NewliquidCrystal/LiquidCrystal_I2C.h"
#include "libraries/OMEEPROM/OMEEPROM.h"
#include "libraries/OMMenuMgr/OMMenuMgr.h"
#include "libraries/Interval/interval.h"
void uiDraw(char* p_text, int p_row, int p_col, int len) ;
void uiLcdPrintSpaces8() ;
void uiScreen() ;
void uiMain() ;
void loadEEPROM() ;
void saveDefaultEEPROM() ;
void setup() ;
bool getInstrumentControl(bool a, uint8_t mode) ;
double analogRead(int pin, int samples);
void test() ;
void loop() ;
void uiResetAction() ;

#include "co2.ino"


#endif
