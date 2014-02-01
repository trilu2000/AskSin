//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Remote Button ---------------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef BUTTONS_H
#define BUTTONS_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "utility/Serial.h"
#include "utility/Fastdelegate.h"
#include "utility/PinChangeIntHandler.h"

class Buttons {
  public://----------------------------------------------------------------------------------------------------------------
	void   config(uint8_t Cnl, uint8_t Pin, uint16_t TimeOutShortDbl, uint16_t LongKeyTime, uint16_t TimeOutLongDdbl, void tCallBack(uint8_t, uint8_t));
	void   interrupt(uint8_t c);
	void   poll(void);

  private://---------------------------------------------------------------------------------------------------------------
	uint16_t toShDbl;																	// minimum time to be recognized as a short key press
	uint16_t lngKeyTme;																	// time key should be pressed to be recognized as a long key press
	uint16_t toLoDbl;																	// maximum time between a double key press
	void (*callBack)(uint8_t, uint8_t);													// call back address for key display

	uint8_t  cFlag :1;																	// was there a change happened in key state
	uint8_t  cStat :1;																	// current status of the button
	uint8_t  idx   :4;																	// stores the channel
	uint32_t cTime;																		// stores the next time to check, otherwise we would check every cycle


	uint8_t  lStat :1;																	// last key state
	uint8_t  dblSh :1;																	// remember last short key press to indicate a double key press
	uint8_t  dblLo :1;																	// remember last long key press to indicate a double key press
	uint8_t  rptLo :1;																	// remember long key press to indicate repeated long, or when long was released
	uint32_t kTime;																		// stores time when the key was pressed or released

	//void   poll_btn(void);																// internal polling function for all inxtStatnces
};


#endif
