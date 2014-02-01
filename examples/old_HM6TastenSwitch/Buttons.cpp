//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Remote Button ---------------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------
#include "Buttons.h"

void Buttons::config(uint8_t Cnl, uint8_t Pin, uint16_t TimeOutShortDbl, uint16_t LongKeyTime, uint16_t TimeOutLongDdbl, void tCallBack(uint8_t, uint8_t)) {

	// settings while setup
	idx = Cnl;																			// set the index in the interrupt array
	pinMode(Pin, INPUT_PULLUP);															// setting the pin to input mode
	toShDbl = TimeOutShortDbl;															// minimum time to be recognized as a short key press
	lngKeyTme  = LongKeyTime;															// time key should be pressed to be recognized as a long key press
	toLoDbl = TimeOutLongDdbl;															// maximum time between a double key press
	callBack = tCallBack;																// call back address for button state display

	// default settings
	cFlag = 0;																			// no need for the poll routine at the moment
	cStat = 1;																			// active low, means last state should be active to get the next change
	lStat = 1;
	dblLo = 0;																			// counter for a double low
	rptLo = 0;																			// counter for repeated low

	registerInt(Pin,s_dlgt(this,&Buttons::interrupt));									// setting the interrupt and port mask
	//	Serial << "pin:" << tPin << ", idx:" << pci.idx[pci.nbr] << ", prt:" << pinPort << ", msk:" << (1 << digitalPinToPCMSKbit(tPin)) << '\n';
}
void Buttons::interrupt(uint8_t c) {
	cStat = c;																			// getting the pin status
	cTime = millis() + 10;																// debounce timer
	cFlag = 1;																			// jump in poll routine next time
}
void Buttons::poll() {
	// possible events of this function:
	//   0 - short key press
	//   1 - double short key press
	//   2 - long key press
	//   3 - repeated long key press
	//   4 - end of long key press
	//   5 - double long key press
	//   6 - time out for double long
	//  
	// 255 - key press, for stay awake issues

	if (cFlag == 0) return;																// no need for do any thing
	if (cTime > millis()) return;														// for debouncing and timeout issues


//	Serial.print("cFlag"); Serial.print(cFlag); Serial.print(", cTime:"); Serial.print(cTime);
//		Serial.print(", cStat:"); Serial.print(cStat); Serial.print(", lStat:"); Serial.print(lStat);
//		Serial.print(", dblLo:"); Serial.print(dblLo); Serial.print(", lngKeyTme:"); Serial.print(kTime);
//		Serial.print(", millis():"); Serial.println(millis());


//	Serial << "cFlag: " << cFlag << ", cTime: " << cTime << ", cStat: " << cStat << ", lStat: " << lStat << ", dblLo: " << dblLo << ", lngKeyTme: " << lngKeyTme << ", kTime: " << kTime << ", millis(): " << millis() << '\n';

	if ((cStat == 1) && (lStat == 1)) {													// timeout
	// only timeouts should happen here

		if ((dblLo) && (kTime + toLoDbl <= millis())) {									// timeout for double long reached
			dblLo = 0;																	// no need for check against
			//Serial.println("dbl lo to");
			callBack(idx,6);															// raise timeout for double long
		}
		if ((dblSh) && (kTime + toShDbl <= millis())) {									// timeout for double short reached
			dblSh = 0;																	// no need for check against
			//Serial.println("dbl sh to");
		}

		if ((dblLo == 0) && (dblSh == 0)) cFlag = 0;									// no need for checking again
		if (dblLo) cTime = millis() + toLoDbl;											// set the next check time
		if (dblSh) cTime = millis() + toShDbl;											// set the next check time

	} else if ((cStat == 1) && (lStat == 0)) {											// key release
	// coming from a short or long key press, end of long key press by checking against rptLo

		if (rptLo) {																	// coming from a long key press
			rptLo = 0;																	// could not be repeated any more
			//Serial.println("end lo");
			callBack(idx,4);															// end of long key press

		} else if (dblSh) {																// double short was set
			dblSh = 0;																	// no need for this flag anymore
			//Serial.println("dbl sh");
			callBack(idx,1);															// double short key press

		} else if (kTime + lngKeyTme > millis()) {										// short key press
			dblSh = 1;																	// next time it could be a double short
			//Serial.println("sh");
			if (!toShDbl) callBack(idx,0);												// short key press
		}
		if ((dblSh) && (toShDbl)) cTime = millis() + toShDbl;							// set the next check time
		if (dblLo) cTime = millis() + toLoDbl;											// set the next check time
		kTime = millis();																// set variable to measure against
		lStat = cStat;																	// remember last key state
		cFlag = 1;																		// next check needed

	} else if ((cStat == 0) && (lStat == 1)) {
	// key is pressed just now, set timeout
		kTime = millis();																// store timer
		cTime = millis() + lngKeyTme;													// set next timeout
		lStat = cStat;																	// remember last key state
		cFlag = 1;																		// next check needed
		callBack(idx,255);
		
	} else if ((cStat == 0) && (lStat == 0)) {
	// next check time while long key press or a repeated long key press
	// if it is a long key press, check against dblLo for detecting a double long key press
		if (rptLo) {																	// repeated long detect
			dblLo = 0;																	// could not be a double any more
			cTime = millis() + 300; //lngKeyTme;										// set next timeout
			//Serial.println("rpt lo");
			callBack(idx,3);															// repeated long key press

		} else if (dblLo) {																// long was set last time, should be a double now
			rptLo = 0;																	// could not be a repeated any more
			dblLo = 0;																	// could not be a double any more
			cFlag = 0;																	// no need for jump in again
			//Serial.println("dbl lo");
			callBack(idx,5);															// double long key press

		} else {																		// first long detect
			dblLo = 1;																	// next time it could be a double
			rptLo = 1;																	// or a repeated long
			cTime = millis() + lngKeyTme;												// set next timeout
			//Serial.println("lo");
			callBack(idx,2);															// long key press

		}
	}
}


