//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Remote Button ---------------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------
#include "Buttons.h"

//- user code here --------------------------------------------------------------------------------------------------------
void MyClassName::config(uint8_t Pin, void tCallBack(uint8_t, uint8_t)) {

	// settings while setup
	pinMode(Pin, INPUT_PULLUP);															// setting the pin to input mode
	callBack = tCallBack;																// call back address for button state display

	// default settings
	cFlag = 0;																			// no need for the poll routine at the moment
	cStat = 1;																			// active low, means last state should be active to get the next change
	lStat = 1;
	dblLo = 0;																			// counter for a double low
	rptLo = 0;																			// counter for repeated low

	registerInt(Pin,s_dlgt(this,&MyClassName::interrupt));								// setting the interrupt and port mask

	if (regCnl) list1 = (s_regChanL1*)ptrMainList;										// assign the pointer to our class struct
	else {
		const uint8_t temp[3] = {0,0,0};
		list1 = (s_regChanL1*)temp;
	}
	configCngEvent();																	// call the config change event for setting up the timing variables
}
void MyClassName::interrupt(uint8_t c) {
	cStat = c;																			// getting the pin status
	cTime = millis() + 10;																// debounce timer
	cFlag = 1;																			// jump in poll routine next time
}

void MyClassName::buttonAction(uint8_t bEvent) {
	// possible events of this function:
	//   0 - short key press
	//   1 - double short key press
	//   2 - long key press
	//   3 - repeated long key press
	//   4 - end of long key press
	//   5 - double long key press
	//   6 - time out for a double long
	//
	// 255 - key press, for stay awake issues

	hm->stayAwake(5000);																// overcome the problem of not getting a long repeated key press
	if (bEvent == 255) return;															// was only a wake up message

	#ifdef DM_DBG
	Serial << "i:" << regCnl << ", s:" << bEvent << '\n';								// some debug message
	#endif
	
	if (regCnl == 0) {																	// config button mode

		if (bEvent == 0) hm->startPairing();											// short key press, send pairing string
		if (bEvent == 2) hm->statusLed.set(STATUSLED_2, STATUSLED_MODE_BLINKSFAST);		// long key press could mean, you like to go for reseting the device
		if (bEvent == 6) hm->statusLed.set(STATUSLED_2, STATUSLED_MODE_OFF);			// time out for double long, stop slow blinking
		if (bEvent == 5) hm->reset();													// double long key press, reset the device

	} else {

		if ((bEvent == 0) || (bEvent == 1)) hm->sendPeerREMOTE(regCnl, 0, 0);			// short key or double short key press detected
		if ((bEvent == 2) || (bEvent == 3)) hm->sendPeerREMOTE(regCnl, 1, 0);			// long or repeated long key press detected
		if (bEvent == 4) hm->sendPeerREMOTE(regCnl, 2, 0);								// end of long or repeated long key press detected
	}
	
	if (callBack) callBack(regCnl, bEvent);												// call the callback function
}

//- mandatory functions for every new module to communicate within HM protocol stack --------------------------------------
void MyClassName::configCngEvent(void) {
	// it's only for information purpose while something in the channel config was changed (List0/1 or List3/4)

	if (regCnl == 0) {																	// we are in config button mode
		
		toShDbl = 0;																	// minimum time to be recognized as a short key press
		lngKeyTme = 5000;																// time key should be pressed to be recognized as a long key press
		toLoDbl = 5000;																	// maximum time between a double key press

	} else {																			// we are in normal button mode

		toShDbl = list1->dblPress*100;													// minimum time to be recognized as a short key press
		lngKeyTme =  300 + (list1->longPress * 100);									// time key should be pressed to be recognized as a long key press
		toLoDbl = 1000;																	// maximum time between a double key press
	}

	#ifdef DM_DBG
	Serial << F("config change; cnl: ") << regCnl << F(", longPress: ") << list1->longPress << F(", sign: ") << list1->sign << F(", dblPress: ") << list1->dblPress  << '\n';
	#endif
}
void MyClassName::pairSetEvent(uint8_t *data, uint8_t len) {
	// we received a message from master to set a new value, typical you will find three bytes in data
	// 1st byte = value; 2nd byte = ramp time; 3rd byte = duration time;
	// after setting the new value we have to send an enhanced ACK (<- 0E E7 80 02 1F B7 4A 63 19 63 01 01 C8 00 54)
	#ifdef DM_DBG
	Serial << F("pairSetEvent, value:") << pHexB(data[0]);
	if (len > 1) Serial << F(", rampTime: ") << pHexB(data[1]);
	if (len > 2) Serial << F(", duraTime: ") << pHexB(data[2]);
	Serial << '\n';
	#endif
		
	hm->sendACKStatus(regCnl,modStat,0);
}
void MyClassName::pairStatusReq(void) {
	// we received a status request, appropriate answer is an InfoActuatorStatus message
	#ifdef DM_DBG
	Serial << F("pairStatusReq\n");
	#endif
	
	hm->sendInfoActuatorStatus(regCnl, modStat, 0);
}
void MyClassName::peerMsgEvent(uint8_t type, uint8_t *data, uint8_t len) {
	// we received a peer event, in type you will find the marker if it was a switch(3E), remote(40) or sensor(41) event
	// appropriate answer is an ACK
	#ifdef DM_DBG
	Serial << F("peerMsgEvent, type: ")  << pHexB(type) << F(", data: ")  << pHex(data,len) << '\n';
	#endif
	
	hm->send_ACK();
}

void MyClassName::poll(void) {
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
			buttonAction(6);															// raise timeout for double long
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
			buttonAction(4);															// end of long key press

		} else if (dblSh) {																// double short was set
			dblSh = 0;																	// no need for this flag anymore
			//Serial.println("dbl sh");
			buttonAction(1);															// double short key press

		} else if (kTime + lngKeyTme > millis()) {										// short key press
			dblSh = 1;																	// next time it could be a double short
			//Serial.println("sh");
			if (!toShDbl) buttonAction(0);												// short key press
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
		buttonAction(255);
		
	} else if ((cStat == 0) && (lStat == 0)) {
	// next check time while long key press or a repeated long key press
	// if it is a long key press, check against dblLo for detecting a double long key press
		if (rptLo) {																	// repeated long detect
			dblLo = 0;																	// could not be a double any more
			cTime = millis() + 300; //lngKeyTme;										// set next timeout
			//Serial.println("rpt lo");
			buttonAction(3);															// repeated long key press

		} else if (dblLo) {																// long was set last time, should be a double now
			rptLo = 0;																	// could not be a repeated any more
			dblLo = 0;																	// could not be a double any more
			cFlag = 0;																	// no need for jump in again
			//Serial.println("dbl lo");
			buttonAction(5);															// double long key press

		} else {																		// first long detect
			dblLo = 1;																	// next time it could be a double
			rptLo = 1;																	// or a repeated long
			cTime = millis() + lngKeyTme;												// set next timeout
			//Serial.println("lo");
			buttonAction(2);															// long key press

		}
	}
}

//- pre defined, no reason to touch ---------------------------------------------------------------------------------------
void MyClassName::regInHM(uint8_t cnl, HM *instPtr) {
	hm = instPtr;																		// set pointer to the HM module
	hm->regCnlModule(cnl,s_mod_dlgt(this,&MyClassName::hmEventCol),(uint16_t*)&ptrMainList,(uint16_t*)&ptrPeerList);
	regCnl = cnl;																		// stores the channel we are responsible fore
}
void MyClassName::hmEventCol(uint8_t by3, uint8_t by10, uint8_t by11, uint8_t *data, uint8_t len) {
	if       (by3 == 0x00)                    poll();
	else if ((by3 == 0x01) && (by11 == 0x06)) configCngEvent();
	else if ((by3 == 0x11) && (by10 == 0x02)) pairSetEvent(data, len);
	else if ((by3 == 0x01) && (by11 == 0x0E)) pairStatusReq();
	else if ((by3 == 0x01) && (by11 == 0x01)) peerAddEvent(data, len);
	else if  (by3 >= 0x3E)                    peerMsgEvent(by3, data, len);
	else return;
}
void MyClassName::peerAddEvent(uint8_t *data, uint8_t len) {
	// we received an peer add event, which means, there was a peer added in this respective channel
	// 1st byte and 2nd byte shows the peer channel, 3rd and 4th byte gives the peer index
	// no need for sending an answer, but we could set default data to the respective list3/4
	#ifdef DM_DBG
	Serial << F("peerAddEvent: pCnl1: ") << pHexB(data[0]) << F(", pCnl2: ") << pHexB(data[1]) << F(", pIdx1: ") << pHexB(data[2]) << F(", pIdx2: ") << pHexB(data[3]) << '\n';
	#endif
	
	if ((data[0]) && (data[1])) {																		// dual peer add
		if (data[0]%2) {																				// odd
			hm->setListFromModule(regCnl,data[2],(uint8_t*)peerOdd,sizeof(peerOdd));
			hm->setListFromModule(regCnl,data[3],(uint8_t*)peerEven,sizeof(peerEven));
		} else {																						// even
			hm->setListFromModule(regCnl,data[2],(uint8_t*)peerEven,sizeof(peerEven));
			hm->setListFromModule(regCnl,data[3],(uint8_t*)peerOdd,sizeof(peerOdd));
		}
	} else {																							// single peer add
		if (data[0]) hm->setListFromModule(regCnl,data[2],(uint8_t*)peerSingle,sizeof(peerSingle));
		if (data[1]) hm->setListFromModule(regCnl,data[3],(uint8_t*)peerSingle,sizeof(peerSingle));
	}
	
}
