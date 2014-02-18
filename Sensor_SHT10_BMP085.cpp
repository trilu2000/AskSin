//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- SHT10 and BMP085 Sensor class -----------------------------------------------------------------------------------------
//- -----------------------------------------------------------------------------------------------------------------------
#include "Sensor_SHT10_BMP085.h"

//- user code here --------------------------------------------------------------------------------------------------------
void MyClassName::config(uint8_t data, uint8_t sck, uint16_t timing, Sensirion *tPtr, BMP085 *pPtr, uint16_t Altidude) {
	tTiming = timing;
	nTime = millis() + 1000;															// set the first time we like to measure
	nAction = 'm';

	sh = tPtr;
	sh->config(7,9);																	// configure the sensor
	sh->writeSR(LOW_RES);																// low resolution is enough

	if (pPtr != NULL) {																	// only if there is a valid module defined
		bm = pPtr;
		bm->begin(0);
		tAlt = Altidude;
	}
}

void MyClassName::poll_measure(void) {
	nTime += measureTime;																// add the 500ms measurement takes to transmit in time
	nAction = 't';

	// measurement code
	// http://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/Humidity/Sensirion_Humidity_SHT1x_Datasheet_V5.pdf
	uint16_t rawData;
	sh->measTemp(&rawData);
	
	if (rawData < 990) tTemp = (rawData*0.4f-396)*-1+0x4000;
	else tTemp = rawData*0.4f-396;
	//Serial << "raw: " << rawData << "  mt: " << tTemp << '\n';
	
	sh->measHumi(&rawData);
	tHum = -2.0468f + 0.5872f * rawData + -4.0845E-4f * rawData * rawData;
	tHum = ((float)tTemp/10 - 25) * (float)(0.01f + 0.00128f * rawData) + tHum;
	//Serial << "raw: " << rawData << "  mH: " << tHum << '\n';
	
	if (bm != NULL) {																	// only if we have a valid module
		tPres = (uint16_t)((bm->readPressure()/100)/pow(1-(tAlt/44330.0),5.255));		// calculate pressure on sea level
		//Serial << " p: " << tPres << '\n';
	}
}
void MyClassName::poll_transmit(void) {
	if (tTiming) nTime = millis() + tTiming;											// there is a given timing
	else nTime = millis() + (calcSendSlot() * 250) - measureTime;						// calculate the next send slot by multiplying with 250ms to get the time in millis
	
	nAction = 'm';																		// next time we want to measure again
	// transmit code
	hm->sendPeerWEATHER(regCnl,tTemp,tHum,tPres);										// send out the weather event
}
uint32_t MyClassName::calcSendSlot(void) {
	uint32_t result = (((hm->getHMID() << 8) | hm->getMsgCnt()) * 1103515245 + 12345) >> 16;
	return (result & 0xFF) + 480;
}

//- mandatory functions for every new module to communicate within HM protocol stack --------------------------------------
void MyClassName::configCngEvent(void) {
	// it's only for information purpose while something in the channel config was changed (List0/1 or List3/4)
	#ifdef DM_DBG
	Serial << F("configCngEvent\n");
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
	// just polling, as the function name said
	if ((nTime == 0) || (nTime > millis())) return;										// check if it is time to jump in

	if      (nAction == 'm') poll_measure();											// measure
	else if (nAction == 't') poll_transmit();											// transmit
}

//- predefined, no reason to touch ----------------------------------------------------------------------------------------
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
