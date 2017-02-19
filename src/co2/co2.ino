#include "Arduino.h"
#include <avr/wdt.h>

#include "libraries/Keypad_I2C/Keypad_I2C.h"
#include "libraries/Keypad/Keypad.h"
#include "libraries/NewliquidCrystal/LiquidCrystal_I2C.h"
#include "libraries/OMEEPROM/OMEEPROM.h"
#include "libraries/OMMenuMgr/OMMenuMgr.h"
#include "libraries/Interval/interval.h"

#define PIN_LED 13

#define PIN_VAL A0
#define PIN_VALUP 9
#define PIN_VALDOWN 10
#define PIN_ALARM 11
#define PIN_BUZZER 4

Interval secInterval;
unsigned long secCnt;

uint8_t uiPage;

double val, valRaw, valHysteresis;
float valLowLimit, valHighLimit, valUpOn, valUpOff, valDownOn, valDownOff;
float valUpPulse, valUpPause, valDownPulse, valDownPause, alarmDelay;
unsigned int valLowX, valHighX;
float valLowY, valHighY;

uint8_t valUpMode, valDownMode, buzzerMode, alarmMode;
bool valUp, valDown, buzzer, alarm, led, valUpAuto, valDownAuto, buzzerAuto, alarmAuto;
bool up, down;
Interval upInterval, downInterval;

/*
char text[16];
char* text1 = "0123456789ABCDEF";
*/

#define LCD_I2CADDR 0x20
const byte LCD_ROWS = 2;
const byte LCD_COLS = 16;

#define KPD_I2CADDR 0x20
const byte KPD_ROWS = 4;
const byte KPD_COLS = 4;

LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

char keys[KPD_ROWS][KPD_COLS] = {
  {'D','C','B','A'},
  {'#','9','6','3'},
  {'0','8','5','2'},
  {'*','7','4','1'}
};

class Alarm {
	bool activated;
	bool deactivated;
	Interval activeInterval;
	Interval deactiveInterval;

public:
	int alarmActiveDelay = 5000;
	int alarmDeactiveDelay = 5000;
	bool active;
	bool unAck;

	bool activate(bool state) {
		if(state) {
			if(!active) {
				if(!activated) {
					activated = true;
					activeInterval.set(alarmActiveDelay);
				}
				if(activeInterval.expired()) {
					activated = false;
					active = true;
					unAck = true;
					return true;
				}
			}
		}
		else
			activated = false;
		return false;
	};
	bool deactivate(bool state) {
		if(state){
			if(active) {
				if(!deactivated) {
					deactivated = true;
					deactiveInterval.set(alarmDeactiveDelay);
				}
				if(deactiveInterval.expired()) {
					deactivated = false;
					active = false;
					return true;
				}
			}
		}
		else
			deactivated = false;
		return false;
	}
	void ack() {
		unAck = false;
	}
};

Alarm valLowAlarm, valHighAlarm, valInvalidAlarm;

class Keypad_I2C2 : public Keypad_I2C {
	unsigned long kTime;
public:
    Keypad_I2C2(char *userKeymap, byte *row, byte *col, byte numRows, byte numCols, byte address, byte width = 1) : Keypad_I2C(userKeymap, row, col, numRows, numCols, address, width) {
    };
    char Keypad_I2C2::getRawKey() {
    	getKeys();
    	for(int r=0; r<sizeKpd.rows; r++)
    		for(int c=0; c<sizeKpd.columns; c++)
    			if((bitMap[r] >> c) & 1)
    				return keymap[r * sizeKpd.columns + c];
    	return NO_KEY;
    };
    char Keypad_I2C2::getKey2() {
    	getKeys();

    	/*
    	//TODO !!! Dirty trick !!!
    	if(bitMap[3] == 8) {
    		if(bitMap[2] == 8) X_start();
    	    if(bitMap[1] == 8) X_stop();
    	    if(bitMap[0] == 8) X_restart();
    		//return NO_KEY;
    	}
    	if(bitMap[3] == 2) {
    		if(bitMap[2] == 8) A_mode=1;
    		if(bitMap[1] == 8) A_mode=2;
    		if(bitMap[0] == 8) A_mode=0;

    		if(bitMap[2] == 4) B_mode=1;
    		if(bitMap[1] == 4) B_mode=2;
    		if(bitMap[0] == 4) B_mode=0;

    		if(bitMap[2] == 2) C_mode=1;
    		if(bitMap[1] == 2) C_mode=2;
    		if(bitMap[0] == 2) C_mode=0;

    		if(bitMap[2] == 1) D_mode=1;
    		if(bitMap[1] == 1) D_mode=2;
    		if(bitMap[0] == 1) D_mode=0;
    		//return NO_KEY;
    	}
    	if(bitMap[3] == 4) {
    		if(bitMap[2] == 8) A_start();
    	    if(bitMap[1] == 8) A_stop();
    	    if(bitMap[0] == 8) A_restart();

    	    if(bitMap[2] == 4) B_start();
    	    if(bitMap[1] == 4) B_stop();
    	    if(bitMap[0] == 4) B_restart();

    	    if(bitMap[2] == 2) C_start();
    	    if(bitMap[1] == 2) C_stop();
    	    if(bitMap[0] == 2) C_restart();

    	    if(bitMap[2] == 1) D_start();
    	    if(bitMap[1] == 1) D_stop();
    	    if(bitMap[0] == 1) D_restart();
    	    //return NO_KEY;
    	}
		*/

    	// for menu system, makes delay between first and next
    	if(bitMap[0] || bitMap[1] || bitMap[2] || bitMap[3]) {
    		if(!kTime)
    			kTime = millis();
    		if((kTime + 500) > millis())
    			if((kTime + 200) < millis())
    				return NO_KEY;
    	}
        else
        	kTime = 0;
    	return getRawKey();
    }
};

byte rowPins[KPD_ROWS] = {0, 1, 2, 3}; //connect to the row pinouts of the keypad
byte colPins[KPD_COLS] = {4, 5, 6, 7}; //connect to the column pinouts of the keypad
Keypad_I2C2 kpd( makeKeymap(keys), rowPins, colPins, KPD_ROWS, KPD_COLS, KPD_I2CADDR, PCF8574 );

class OMMenuMgr2 : public OMMenuMgr {
public:
    OMMenuMgr2(const OMMenuItem* c_first, uint8_t c_type, Keypad_I2C2* c_kpd) :OMMenuMgr( c_first, c_type) {
      kpd = c_kpd;
    };
    int OMMenuMgr2::_checkDigital() {
    	char k = kpd->getKey2();
    	if(k == 'A') return BUTTON_INCREASE;
    	if(k == 'B') return BUTTON_DECREASE;
    	if(k == 'D') return BUTTON_FORWARD;
    	if(k == '#') return BUTTON_BACK;
    	if(k == '*') return BUTTON_SELECT;

    	return k;
    	return BUTTON_NONE;
    }
private:
    Keypad_I2C2* kpd;
};

//CZ
#define TEXT_AUTO_EM			"AUTO!"
#define TEXT_OFF_EM				"VYPNI!"
#define TEXT_ON_EM 				"ZAPNI!"
#define TEXT_START_EM 			"STOP!"
#define TEXT_STOP_EM 			"START!"


//EEPROM
#define ADDR_VALLOWX	 		20
#define ADDR_VALHIGHX	 		24
#define ADDR_VALLOWY	 		28
#define ADDR_VALHIGHY	 		32
#define ADDR_VALUPON	 		36
#define ADDR_VALUPOFF	 		40
#define ADDR_VALDOWNON	 		44
#define ADDR_VALDOWNOFF 		48
#define ADDR_VALLOWLIMIT		52
#define ADDR_VALHIGHLIMIT 		56
#define ADDR_VALUPPULSE 		60
#define ADDR_VALUPPAUSE 		64
#define ADDR_VALDOWNPULSE 		68
#define ADDR_VALDOWNPAUSE 		72
#define ADDR_VALHYSTER	 		76
#define ADDR_ALARMDELAY	 		80


// Create a list of states and values for a select input
MENU_SELECT_ITEM  sel_auto= { 0, {TEXT_AUTO_EM} };
MENU_SELECT_ITEM  sel_off = { 1, {TEXT_OFF_EM} };
MENU_SELECT_ITEM  sel_on  = { 2, {TEXT_ON_EM} };

MENU_SELECT_ITEM  sel_stop= { 0, {TEXT_START_EM} };
MENU_SELECT_ITEM  sel_start={ 1, {TEXT_STOP_EM} };

//MENU_SELECT_ITEM  sel_passive={ 0, {TEXT_PASSIVE} };
//MENU_SELECT_ITEM  sel_active= { 1, {TEXT_ACTIVE} };

MENU_SELECT_LIST  const stateMode_list[] = { &sel_auto, &sel_off, &sel_on};
MENU_SELECT_LIST  const stateOffOn_list[] = { &sel_off, &sel_on };
//MENU_SELECT_LIST  const statePasAct_list[] = { &sel_passive, &sel_active };

/*
MENU_ITEM X_start_item   = { {TEXT_START_ALL_EM},  ITEM_ACTION, 0,        MENU_TARGET(&X_uiStart) };
MENU_ITEM X_stop_item   =  { {TEXT_STOP_ALL_EM },  ITEM_ACTION, 0,        MENU_TARGET(&X_uiStop)  };
MENU_ITEM X_restart_item  ={ {TEXT_RESTART_ALL_EM},  ITEM_ACTION, 0,        MENU_TARGET(&X_uiRestart) };

MENU_ITEM reset_item  ={ {TEXT_RESETPARAM_EM},  ITEM_ACTION, 0,        MENU_TARGET(&uiResetParam) };
*/
//MENU_ITEM setWifiPass_item  ={ {TEXT_SETWIFIPASS},  ITEM_ACTION, 0,        MENU_TARGET(&uiSetWifiPass) };

//MENU_SELECT A_state_select = { &A_state,           MENU_SELECT_SIZE(stateStopStart_list),   MENU_TARGET(&stateStopStart_list) };
//MENU_VALUE A_state_value =   { TYPE_SELECT,     0,     0,     MENU_TARGET(&A_state_select), 0};
//MENU_ITEM A_state_item    =  { {"A STATE:"}, ITEM_VALUE,  0,        MENU_TARGET(&A_state_value) };


// measurement calibration
//                               TYPE             MAX    MIN    TARGET
MENU_VALUE valLowX_value = 	   { TYPE_INT,       1023,  0,     MENU_TARGET(&valLowX), 	ADDR_VALLOWX };
MENU_VALUE valHighX_value =    { TYPE_INT,       1023,  0,     MENU_TARGET(&valHighX), ADDR_VALHIGHX };
MENU_VALUE valLowY_value =	   { TYPE_FLOAT_100,      100,   0,     MENU_TARGET(&valLowY),	ADDR_VALLOWY };
MENU_VALUE valHighY_value =	   { TYPE_FLOAT_100,      100,   0,     MENU_TARGET(&valHighY), ADDR_VALHIGHY };
//                              "123456789ABCDEF"
MENU_ITEM valLowX_item   =   { {"X0          [-]"},   	ITEM_VALUE,  0,        MENU_TARGET(&valLowX_value) };
MENU_ITEM valHighX_item   =  { {"X100        [-]"},      ITEM_VALUE,  0,        MENU_TARGET(&valHighX_value) };
MENU_ITEM valLowY_item  =    { {"Y0          [%]"},		ITEM_VALUE,  0,        MENU_TARGET(&valLowY_value) };
MENU_ITEM valHighY_item   =  { {"Y100        [%]"},   	ITEM_VALUE,  0,        MENU_TARGET(&valHighY_value) };

MENU_LIST const submenu_list_val[] = {&valLowX_item, &valLowY_item,  &valHighX_item, &valHighY_item};
MENU_ITEM menu_submenu_val = { {"KALIBRACE->"},  ITEM_MENU,  MENU_SIZE(submenu_list_val),  MENU_TARGET(&submenu_list_val) };

// actuator up
MENU_SELECT valUpMode_select ={ &valUpMode,           MENU_SELECT_SIZE(stateMode_list),   MENU_TARGET(&stateMode_list) };
MENU_VALUE valUpMode_value =  { TYPE_SELECT,     0,     0,     MENU_TARGET(&valUpMode_select) };
MENU_ITEM valUpMode_item    = { {"RELE->"}, ITEM_VALUE,  0,        MENU_TARGET(&valUpMode_value) };
MENU_VALUE valUpOn_value =      { TYPE_FLOAT_100, 100,     0,    MENU_TARGET(&valUpOn), ADDR_VALUPON  };
MENU_VALUE valUpOff_value=      { TYPE_FLOAT_100, 100,     0,    MENU_TARGET(&valUpOff), ADDR_VALUPOFF };
//                          "123456789ABCDEF"
MENU_ITEM valUpOn_item  ={ {"ZAPNI       [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valUpOn_value)};
MENU_ITEM valUpOff_item ={ {"VYPNI       [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valUpOff_value) };

MENU_VALUE valUpPulse_value =      { TYPE_FLOAT_10, 0,     0,    MENU_TARGET(&valUpPulse), ADDR_VALUPPULSE  };
MENU_VALUE valUpPause_value=      { TYPE_FLOAT_10, 0,     0,    MENU_TARGET(&valUpPause), ADDR_VALUPPAUSE };
//                             "123456789ABCDEF"
MENU_ITEM valUpPulse_item  ={ {"PULZ        [S]"},    ITEM_VALUE,  0,        MENU_TARGET(&valUpPulse_value)};
MENU_ITEM valUpPause_item = { {"PAUSA       [S]"},    ITEM_VALUE,  0,        MENU_TARGET(&valUpPause_value) };

MENU_LIST const submenu_list_valUp[] = {&valUpMode_item, &valUpOn_item,  &valUpOff_item, &valUpPulse_item, &valUpPause_item};
MENU_ITEM menu_submenu_valUp = { {"RELE VICE->"},  ITEM_MENU,  MENU_SIZE(submenu_list_valUp),  MENU_TARGET(&submenu_list_valUp) };

// actuator down
MENU_SELECT valDownMode_select ={ &valDownMode,           MENU_SELECT_SIZE(stateMode_list),   MENU_TARGET(&stateMode_list) };
MENU_VALUE valDownMode_value =  { TYPE_SELECT,     0,     0,     MENU_TARGET(&valDownMode_select) };
MENU_ITEM valDownMode_item    = { {"RELE->"}, ITEM_VALUE,  0,        MENU_TARGET(&valDownMode_value) };
MENU_VALUE valDownOn_value =      { TYPE_FLOAT_100, 100,     0,    MENU_TARGET(&valDownOn), ADDR_VALDOWNON  };
MENU_VALUE valDownOff_value=      { TYPE_FLOAT_100, 100,     0,    MENU_TARGET(&valDownOff), ADDR_VALDOWNOFF };
//                            "123456789ABCDEF"
MENU_ITEM valDownOn_item  ={ {"ZAPNI       [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valDownOn_value)};
MENU_ITEM valDownOff_item ={ {"VYPNI       [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valDownOff_value) };

MENU_VALUE valDownPulse_value =      { TYPE_FLOAT_10, 0,     0,    MENU_TARGET(&valDownPulse), ADDR_VALDOWNPULSE  };
MENU_VALUE valDownPause_value=      { TYPE_FLOAT_10, 0,     0,    MENU_TARGET(&valDownPause), ADDR_VALDOWNPAUSE };
//                           	 "123456789ABCDEF"
MENU_ITEM valDownPulse_item  ={ {"PULZ        [S]"},    ITEM_VALUE,  0,        MENU_TARGET(&valDownPulse_value)};
MENU_ITEM valDownPause_item = { {"PAUZA       [S]"},    ITEM_VALUE,  0,        MENU_TARGET(&valDownPause_value) };

MENU_LIST const submenu_list_valDown[] = {&valDownMode_item, &valDownOn_item,  &valDownOff_item, &valDownPulse_item, &valDownPause_item};
MENU_ITEM menu_submenu_valDown = { {"RELE MENE->"},  ITEM_MENU,  MENU_SIZE(submenu_list_valDown),  MENU_TARGET(&submenu_list_valDown) };

// alarm
MENU_SELECT alarmMode_select ={ &alarmMode,           MENU_SELECT_SIZE(stateMode_list),   MENU_TARGET(&stateMode_list) };
MENU_VALUE alarmMode_value =  { TYPE_SELECT,     0,     0,     MENU_TARGET(&alarmMode_select) };
MENU_ITEM alarmMode_item    = { {"RELE ALARM->"}, ITEM_VALUE,  0,        MENU_TARGET(&alarmMode_value) };

MENU_SELECT buzzerMode_select ={ &buzzerMode,           MENU_SELECT_SIZE(stateMode_list),   MENU_TARGET(&stateMode_list) };
MENU_VALUE buzzerMode_value =  { TYPE_SELECT,     0,     0,     MENU_TARGET(&buzzerMode_select) };
MENU_ITEM buzzerMode_item    = { {"BZUCAK->"}, ITEM_VALUE,  0,        MENU_TARGET(&buzzerMode_value) };

MENU_VALUE valHighLimit_value=	{ TYPE_FLOAT_100, 100,    0,    MENU_TARGET(&valHighLimit), ADDR_VALHIGHLIMIT };
MENU_ITEM valHighLimit_item   =	{ {"VYSOKA KONC [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valHighLimit_value) };
MENU_VALUE valLowLimit_value=	{ TYPE_FLOAT_100, 100,    0,    MENU_TARGET(&valLowLimit), ADDR_VALLOWLIMIT };
MENU_ITEM valLowLimit_item   =	{ {"NIZKA KONCE [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valLowLimit_value) };

MENU_VALUE alarmDelay_value=      { TYPE_FLOAT_100, 100,     0,    MENU_TARGET(&alarmDelay), ADDR_ALARMDELAY };
//                            "123456789ABCDEF"
MENU_ITEM alarmDelay_item ={ {"ZPOZ ALARMU [S]"},    ITEM_VALUE,  0,        MENU_TARGET(&alarmDelay_value) };

MENU_VALUE valHysteresis_value =      { TYPE_FLOAT_100, 100,     0,    MENU_TARGET(&valHysteresis), ADDR_VALHYSTER  };
//                                "123456789ABCDEF"
MENU_ITEM valHysteresis_item  ={ {"HYSTEREZE   [%]"},    ITEM_VALUE,  0,        MENU_TARGET(&valHysteresis_value)};

MENU_LIST const submenu_list_valLimit[] = {&valLowLimit_item, &valHighLimit_item, &valHysteresis_item, &alarmDelay_item, &alarmMode_item, &buzzerMode_item};
MENU_ITEM menu_submenu_valLimit = { {"ALARM->"},  ITEM_MENU,  MENU_SIZE(submenu_list_valLimit),  MENU_TARGET(&submenu_list_valLimit) };

MENU_ITEM item_reset   = { {"RESET!"},  ITEM_ACTION, 0,        MENU_TARGET(&uiResetAction) };

// root
MENU_LIST const root_list[]   = { &menu_submenu_val, &menu_submenu_valUp, &menu_submenu_valDown, &menu_submenu_valLimit, &item_reset};
MENU_ITEM menu_root     = { {"ROOT"},        ITEM_MENU,   MENU_SIZE(root_list),    MENU_TARGET(&root_list) };



OMMenuMgr2 Menu(&menu_root, MENU_DIGITAL, &kpd);

void uiDraw(char* p_text, int p_row, int p_col, int len) {
	lcd.backlight();
	lcd.setCursor(p_col, p_row);
	for( int i = 0; i < len; i++ ) {
		if( p_text[i] < '!' || p_text[i] > '~' )
			lcd.write(' ');
		else
			lcd.write(p_text[i]);
	}
}

void uiLcdPrintSpaces8() {
	lcd.print(F("        "));
}

/*
void uiInstrument(char* name, bool out, uint8_t mode, uint8_t state, unsigned long cycles, unsigned long cyclesLimit, bool detail=true) {
	//lcd.clear();
	if(detail)
		lcd.setCursor(0, 0);
	lcd.print(name);
	if(!detail)
		if(secCnt & 1)
			lcd.print(':');
		else
			lcd.print(' ');


	if(detail) {
		//if(A_outPin)
		if(out)
			lcd.print(F(TEXT_ON));
		else
			lcd.print(F(TEXT_OFF));
		if(mode)
			lcd.print(F(TEXT_MAN));
		else
			lcd.print(F(TEXT_AUTO));
		if(state)
			lcd.print(F(TEXT_ACT));
		else
			lcd.print(F(TEXT_PAS));
		if(X_state)
			lcd.print(F(TEXT_RUN));
		else
			lcd.print(F(TEXT_STOP));
	}
	else {
		//if(A_outPin)
		if(out)
			lcd.print(F(TEXT_ON2));
		else
			lcd.print(F(TEXT_OFF2));
		if(!mode && state && X_state)
			lcd.print(F(TEXT_RUN2));
		else
			lcd.print(F(TEXT_STOP2));

	if(detail) {
		lcd.setCursor(0, 1);
		lcd.print(cycles);

		if(secCnt & 1)
			lcd.print(':');
		else
			lcd.print(' ');

		lcd.print(cyclesLimit);
		uiLcdPrintSpaces8();
		uiLcdPrintSpaces8();
	}
}
*/

//TODO
uint8_t uiState;
enum {UISTATE_MAIN, UISTATE_EDITTEXT };

char uiChar;
bool uiMenuBlocked;
//



void uiScreen() {

	//lcd.clear();
	//uiLcdPrintSpaces8();

	//lcd.setCursor(8, 0);




	Menu.enable(false);

	if(valLowAlarm.unAck || valHighAlarm.unAck || valInvalidAlarm.unAck) {
		(secCnt & 1) ? lcd.backlight() : lcd.noBacklight();
	}
	else {
		lcd.backlight();
	}


	if(uiState == UISTATE_MAIN) {

		if(uiChar =='A') { //KPD_UP)
			lcd.clear();
			uiPage--;
		}
		if(uiChar == 'B') { //KPD_DOWN)
			lcd.clear();
			uiPage++;
		}
		if(uiChar == '#') {//KPD_DOWN) {
			lcd.clear();
			uiPage = 0;
		}

		uiPage = max(0, uiPage);
		uiPage = min(2, uiPage);

		if(uiPage==0) {

			lcd.setCursor(0, 0);
			lcd.print("CO2");
			//uiLcdPrintSpaces8();
			if(secCnt & 1)
				lcd.print(':');
			else
				lcd.print(' ');
			if(val < 10.0) {
				lcd.print(' ');
				lcd.setCursor(5, 0);
			}
			else
				lcd.setCursor(4, 0);
			lcd.print(val);
			lcd.setCursor(9, 0);
			lcd.print("% ");

			lcd.setCursor(11, 0);
			if(up)
				lcd.print(F("VICE"));
			else if(down)
				lcd.print(F("MENE"));
			else
				uiLcdPrintSpaces8();

			if(valUp || valDown)
				lcd.print(F("1"));
			else
				lcd.print(F("0"));
			lcd.setCursor(0, 1);

			if(valInvalidAlarm.active)
				lcd.print(F("CHYBA MERENI!   "));
			else if(valLowAlarm.active)
				lcd.print(F("NIZKA KONCENTRA!"));
			else if(valHighAlarm.active)
				lcd.print(F("VYSOKA KONCENTR!"));
			else {
				uiLcdPrintSpaces8();
				uiLcdPrintSpaces8();
			}

			/*
			//lcd.setCursor(0, 0);
			//lcd.print("-");
			//uiLcdPrintSpaces8();
			//uiLcdPrintSpaces8();
			lcd.setCursor(0, 0);
			uiInstrument("A", A_outPin, A_mode, A_state, A_halfCycles>>1, A_cyclesLimit, false);
			lcd.print("  ");

			//if(secCnt & 1)
			//	lcd.print(':');
			//else
			//	lcd.print(' ');

			uiInstrument("B", B_outPin, B_mode, B_state, B_halfCycles>>1, B_cyclesLimit, false);

			lcd.setCursor(0, 1);
			uiInstrument("C", C_outPin, C_mode, C_state, C_halfCycles>>1, C_cyclesLimit, false);
			lcd.print("  ");

			//if(secCnt & 1)
			//	lcd.print(' ');
			//else
			//	lcd.print(':');

			uiInstrument("D", D_outPin, D_mode, D_state, D_halfCycles>>1, D_cyclesLimit, false);
			*/
		}
		else if(uiPage==1) {
			lcd.setCursor(0, 0);
			lcd.print("X[-]");
			lcd.setCursor(0, 1);
			lcd.print(valRaw);
			uiLcdPrintSpaces8();
		}
		else if(uiPage==2) {
			lcd.setCursor(0, 0);
			lcd.print("EMAIL  BCSEDLON@");
			lcd.setCursor(0, 1);
			lcd.print("       GMAIL.COM");
		}
	}


	//lcd.backlight();
	/*
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print('|');
	lcd.print('-');
	*/

	/*
	if(uiState == UISTATE_EDITTEXT) {

		lcd.setCursor(0, 0);
		//"0123456789ABCDEF"
		lcd.print(F("TEXT:"));

		if(uiChar == 'C')
			uiPage++;
		if(uiChar == 'D')
			uiPage--;

		uiPage = max(0, uiPage);
		uiPage = min(15, uiPage);

		//text[0] = 64;
		uint8_t i;

		//strncpy(text2, text, 16);
		i = text[uiPage];
		if(uiChar == 'A') i++;
		if(uiChar == 'B') i--;
		i = max(32, i);
		i = min(126, i);
		text[uiPage] = (char)i;

		lcd.setCursor(0, 1);
		lcd.print(text);

		uiLcdPrintSpaces8();
		uiLcdPrintSpaces8();

		if(secCnt & 1) {
			lcd.setCursor(uiPage, 1);
			lcd.print('_');
		}

		//lcd.setCursor(uiPage, 1);
		//lcd.print(char(i));

		//Serial.println(uiChar);
		//Serial.println();
		//Serial.println(uiPage);
		//Serial.println(i);
		//Serial.println();

		if(uiChar == '*') {
			strncpy(text1, text, 16);
			//uiState = UISTATE_MAIN;
			//uiMenuBlocked = false;
			//Menu.enable(true);
			//uiState=0;
		}

		if(uiChar == '#') {
			uiMenuBlocked = false;
			//Menu.enable(true);
			uiState = UISTATE_MAIN;
		}



	}*/
	uiChar = 0;
}

void uiMain() {
	lcd.clear();
	//uiState = UISTATE_MAIN;
	uiPage = 0;
	uiScreen();
}

/*
void listener(char ch) {
	Serial.println(ch);
}
*/


void loadEEPROM() {
    using namespace OMEEPROM;
    read(ADDR_VALLOWX, valLowX);
    read(ADDR_VALHIGHX, valHighX);
    read(ADDR_VALLOWY, valLowY);
    read(ADDR_VALHIGHY, valHighY);
    read(ADDR_VALUPON, valUpOn);
    read(ADDR_VALUPOFF, valUpOff);
    read(ADDR_VALDOWNON, valDownOn);
    read(ADDR_VALDOWNOFF, valDownOff);
    read(ADDR_VALLOWLIMIT, valLowLimit);
    read(ADDR_VALHIGHLIMIT, valHighLimit);
    read(ADDR_VALUPPULSE, valUpPulse);
    read(ADDR_VALUPPAUSE, valUpPause);
    read(ADDR_VALDOWNPULSE, valDownPulse);
    read(ADDR_VALDOWNPAUSE, valDownPause);
    read(ADDR_VALHYSTER, valHysteresis);
    read(ADDR_ALARMDELAY, alarmDelay);
}

#define nominal 5.0
void saveDefaultEEPROM() {

	valLowX = 174;
	valHighX = 250;
	valLowY = 0.04;
	valHighY = 4.0;

	valUpOn = nominal-0.1;
	valUpOff = nominal-0.2;
	valLowLimit = nominal-0.3;
	valUpPulse = 1.0;
	valUpPause = 2.0;

	valHighLimit = nominal+0.3;
	valDownOn = nominal+0.2;
	valDownOff = nominal+0.1;
	valDownPulse = 1.0;
	valDownPause = 2.0;

	valHysteresis = 0.1;
	alarmDelay = 5.0;

    using namespace OMEEPROM;
    write(ADDR_VALLOWX, valLowX);
    write(ADDR_VALHIGHX, valHighX);
    write(ADDR_VALLOWY, valLowY);
    write(ADDR_VALHIGHY, valHighY);
    write(ADDR_VALUPON, valUpOn);
    write(ADDR_VALUPOFF, valUpOff);
    write(ADDR_VALDOWNON, valDownOn);
    write(ADDR_VALDOWNOFF, valDownOff);
    write(ADDR_VALLOWLIMIT, valLowLimit);
    write(ADDR_VALHIGHLIMIT, valHighLimit);
    write(ADDR_VALUPPULSE, valUpPulse);
    write(ADDR_VALUPPAUSE, valUpPause);
    write(ADDR_VALDOWNPULSE, valDownPulse);
    write(ADDR_VALDOWNPAUSE, valDownPause);
    write(ADDR_VALHYSTER, valHysteresis);
    write(ADDR_ALARMDELAY, alarmDelay);
}

//The setup function is called once at startup of the sketch
void setup()
{
	// Add your initialization code here
	Serial.begin(128000);
		while(!Serial);

	//Serial.println("CLEARDATA"); //clears up any data left from previous projects
	//Serial.println("LABEL,No.,CO2,..."); //always write LABEL, so excel knows the next things will be the names of the columns (instead of Acolumn you could write Time for instance)
	//Serial.println("RESETTIMER"); //resets timer to 0

	//Serial.begin(128000); // opens serial port, sets data rate128000 bps
	Serial.println("CLEARDATA"); //clears any residual data
	Serial.println("LABEL,Time,No.,CO2");

	if( OMEEPROM::saved() )
		loadEEPROM();
	else
		saveDefaultEEPROM();

	Wire.begin( );
	kpd.begin( makeKeymap(keys) );
	//kpd.setDebounceTime(10);
	//kpd.setHoldTime(120);
	//kpd.addEventListener(listener);

	lcd.begin(LCD_COLS, LCD_ROWS);

	Menu.setDrawHandler(uiDraw);
	Menu.setExitHandler(uiMain);
	Menu.enable(true);

	uiMain();

	pinMode(PIN_LED, OUTPUT);

	digitalWrite(PIN_VALUP, true);
	digitalWrite(PIN_VALDOWN, true);
	digitalWrite(PIN_BUZZER, true);
	digitalWrite(PIN_ALARM, true);

	pinMode(PIN_VALUP, OUTPUT);
	pinMode(PIN_VALDOWN, OUTPUT);
	pinMode(PIN_BUZZER, OUTPUT);
	pinMode(PIN_ALARM, OUTPUT);

	wdt_enable(WDTO_2S);

	// Timer0 is already used for millis() - we'll just interrupt somewhere
	// in the middle and call the "Compare A" function below
	//OCR0A = 0xAF;
	//TIMSK0 |= _BV(OCIE0A);
}





/*
// Interrupt is called once a millisecond,
SIGNAL(TIMER0_COMPA_vect)
{
	//run every sec
		if(X_state) {
			//running
			if(A_state) {
				if(A_out)
					A_set = A_onSec + A_onMin*60 + A_onHour*60*60;
				else
					A_set = A_offSec + A_offMin*60 + A_offHour*60*60;
				if(A_set > A_sec) {
					A_out = ~A_out;
					A_sec = 0;
				}
				else
					A_sec++;
			}

		}

}
*/

/*
void uiSetWifiPass() {
	Menu.enable(false);
	uiMenuBlocked = true;
	uiState = UISTATE_EDITTEXT;
	uiPage=0;
	lcd.clear();

	strncpy(text, text1, 16);
}
*/

bool getInstrumentControl(bool a, uint8_t mode) {
	if(mode == 0) return a;
	if(mode == 1) return false;
	if(mode == 2) return true;
	return false;
}

// The loop function is called in an endless loop



double analogRead(int pin, int samples){
  int result = 0;
  for(int i=0; i<samples; i++){
    result += analogRead(pin);
  }
  return (double)(result / samples);
}

uint i, cnt;
Interval intv;
void test()
{
	wdt_reset();
	valRaw = analogRead(PIN_VAL, 100);
	double valRatio = (double)(valHighY - valLowY) / (double)(valHighX - valLowX);
	//val = ((double)valHighX - valRaw) / valRatio + (double)valLowY;
	val = ((valRaw - valLowX) * valRatio + (double)valLowY);
	//val += ( 0.1 * ((valRaw - valLowX) * valRatio + (double)valLowY) - val);
	//val = valLowLimit;
	val = max(val, 0);

	//Serial.println(millis() -i);
	//i = millis();// - i;
	//Serial.println(val);

	//Serial.print("DATA,TIME,");
	//Serial.print(i++);
	//Serial.print(",");
	if(intv.expired()) {
		intv.set(1000);
		Serial.println(i++);
		Serial.println(cnt);
		cnt = 0;
	}
	cnt++;
	Serial.println(val);
}


void loop() {

	wdt_reset();

	//Add your repeated code here
	//char key = kpd.getKey();
	//char ch = kpd.getRawKey();
	char ch = kpd.getKey2();
	if(ch) {
		valLowAlarm.ack();
		valHighAlarm.ack();
		valInvalidAlarm.ack();
	}

	if(ch == '*') {//KPD_ENTER)
		//if(!uiState)
		if(!uiMenuBlocked)
			Menu.enable(true);
		//uiState = 0;
		//uiPage = 0;
	}
	/*
	if(ch == '#')
		if(uiState) {
			Menu.enable(true);
			uiState = 0;
		}
	*/

	Menu.checkInput();

	valRaw = analogRead(PIN_VAL, 100);

	double valRatio = (double)(valHighY - valLowY) / (double)(valHighX - valLowX);
	//val = ((double)valHighX - valRaw) / valRatio + (double)valLowY;
	val = ((valRaw - valLowX) * valRatio + (double)valLowY);
	//val += ( 0.1 * ((valRaw - valLowX) * valRatio + (double)valLowY) - val);
	//val = valLowLimit;
	val = max(val, 0);

	//Serial.println(val);

	//Serial.print("DATA,TIME,TIMER,"); //writes the time in the first column A and the time since the measurements started in column B
	//Serial.print(i);
	//Serial.print(val);
	//Serial.println(); //be sure to add println to the last command so it knows to go into the next row on the second run
	//delay(100); //add a delay

	//Serial.print("DATA,TIME,");
	//Serial.print(i++);
	//Serial.print(",");
	//Serial.println(val);


	valLowAlarm.alarmActiveDelay = alarmDelay * 1000;
	valLowAlarm.alarmDeactiveDelay = alarmDelay * 1000;
	valHighAlarm.alarmActiveDelay = alarmDelay * 1000;
	valHighAlarm.alarmDeactiveDelay = alarmDelay * 1000;

	valLowAlarm.activate(val <= valLowLimit);
	valLowAlarm.deactivate(val >= valLowLimit + valHysteresis);

	valHighAlarm.activate(val >= valHighLimit);
	valHighAlarm.deactivate(val <= valHighLimit - valHysteresis);

	if (secInterval.expired()) {
		secInterval.set(1000);
		secCnt++;

		led = !led;
		digitalWrite(PIN_LED, led);
	}

	// up
	if(val <= valUpOn)
		up = true;
	if(val >= valUpOff)
		up = false;
	if(up) {
		if(upInterval.expired()) {
			if(!valUpAuto) {
				upInterval.set(valUpPulse * 1000);
				valUpAuto = true;
			}
			else {
				upInterval.set(valUpPause * 1000);
				valUpAuto = false;
			}
		}
		if(valUpPulse == 0)
			valUpAuto = false;
		if(valUpPause == 0)
			valUpAuto = true;
	}
	else {
		upInterval.set(0);
		valUpAuto = false;
	}

	// down
	if(val >= valDownOn)
		down = true;
	if(val <= valDownOff)
		down = false;
	if(down) {
		if(downInterval.expired()) {
			if(!valDownAuto) {
				downInterval.set(valDownPulse * 1000);
				valDownAuto = true;
			}
			else {
				downInterval.set(valDownPause * 1000);
				valDownAuto = false;
			}
		}
		if(valDownPulse == 0)
			valDownAuto = false;
		if(valDownPause == 0)
			valDownAuto = true;
	}
	else {
		downInterval.set(0);
		valDownAuto = false;
	}

	valInvalidAlarm.activate(valRaw < 100);
	valInvalidAlarm.deactivate(valRaw >= 100);
	if(valInvalidAlarm.active) {
		valUpAuto = false;
		valDownAuto = false;
		up = false;
		down = false;
	}


	alarmAuto = valLowAlarm.active | valHighAlarm.active | valInvalidAlarm.active;
	buzzerAuto = (valLowAlarm.unAck | valHighAlarm.unAck | valInvalidAlarm.unAck) & (secCnt & 1);

	valUp = getInstrumentControl(valUpAuto, valUpMode);
	valDown = getInstrumentControl(valDownAuto, valDownMode);
	buzzer = getInstrumentControl(buzzerAuto, buzzerMode);
	alarm = getInstrumentControl(alarmAuto, alarmMode);

	digitalWrite(PIN_VALUP, !valUp);
	digitalWrite(PIN_VALDOWN, !valDown);
	digitalWrite(PIN_BUZZER, !buzzer);
	digitalWrite(PIN_ALARM, !alarm);

	if(!Menu.shown() || !Menu.enable()) {
	//if(!Menu.enable()) {
		uiScreen();

		ch = kpd.getKey();
		//ch = kpd.getKey2();
		/*
		if(ch =='A') //KPD_UP)
			uiPage--;
		if(ch == 'B') //KPD_DOWN)
			uiPage++;
		if(ch == '#') {//KPD_DOWN)
			uiPage = 0;
			//uiMenuBlocked = false;
		}
		*/
		//uiPage = max(0, uiPage);
		//uiPage = min(5, uiPage);

		if(uiChar==0)
			uiChar = ch;

	}
}

void uiResetAction() {
	saveDefaultEEPROM();

	lcd.clear();
	lcd.print(F("RESETOVANO"));
	delay(1000);

	loadEEPROM();
}
